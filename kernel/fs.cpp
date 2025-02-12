#include "kernel/fs"

#include <cstdint>
#include <cstring>

#include "bio"
#include "file"
#include "fmt"
#include "fs"
#include "lock"
#include "log"
#include "proc"
#include "virtio_disk"

namespace fs {

struct superblock sb;

auto read_supblock(int dev, struct superblock *supblock) -> void {
  auto *b = bio::bread(dev, 1);
  std::memmove(supblock, b->data, sizeof(*supblock));
  bio::brelse(*b);
}

auto init(int dev) -> void {
  read_supblock(dev, &sb);
  if (sb.magic != FSMAGIC) {
    fmt::panic("fs::init: invalid file system");
  }
  log::init(dev, sb);
}

// for Block

auto bzero(uint32_t dev, int bno) -> void {
  auto *bp = bio::bread(dev, bno);
  std::memset(bp->data, 0, fs::BSIZE);
  log::lwrite(bp);
  bio::brelse(*bp);
}

auto balloc(uint32_t dev) -> uint32_t {
  for (uint32_t b = 0; b < sb.size; b += BPB) {
    auto *bp = bio::bread(dev, BBLOCK(b, sb));
    for (uint32_t bi = 0; bi < BPB && b + bi < sb.size; ++bi) {
      auto m = 1U << (bi % 8);
      if ((bp->data[bi / 8] & m) == 0) {
        bp->data[bi / 8] |= m;
        log::lwrite(bp);
        bio::brelse(*bp);
        bzero(dev, b + bi);
        return b + bi;
      }
    }
    bio::brelse(*bp);
  }
  fmt::print_log(fmt::log_level::WARNNING, "balloc: out of blocks\n");
  return 0;
}

auto bfree(uint32_t dev, uint32_t b) {
  auto *bp = bio::bread(dev, BBLOCK(b, sb));
  auto bi = b % BPB;
  auto m = 1U << (bi % 8);

  if ((bp->data[bi % 8] & m) == 0) {
    fmt::panic("fs::bfree: it's free");
  }

  bp->data[bi / 8] &= ~m;
  log::lwrite(bp);
  bio::brelse(*bp);
}

struct {
  struct lock::spinlock lock;
  struct file::inode inode[fs::NINODE];
} itable;

auto iinit() -> void {
  lock::spin_init(itable.lock, (char *)"itable");
  for (auto &i : itable.inode) {
    lock::sleep_init(i.lock, (char *)"inode");
  }
}

auto iget(uint32_t dev, uint32_t inum) -> struct file::inode *;
auto itrunc(struct file::inode *ip) -> void;

auto ialloc(uint32_t dev, int16_t type) -> struct file::inode * {
  for (uint32_t inum = 1; inum < sb.ninodes; ++inum) {
    auto *bp = bio::bread(dev, IBLOCK(inum, sb));
    auto *dip = (struct dinode *)bp->data + inum % IPB;
    if (dip->type == 0) {
      std::memmove(dip, 0, sizeof(*dip));
      dip->type = type;
      log::lwrite(bp);
      bio::brelse(*bp);
      return iget(dev, inum);
    }
    bio::brelse(*bp);
  }

  fmt::print_log(fmt::log_level::WARNNING, "fs::ialloc: no inodes");
  return nullptr;
}

auto iupdate(struct file::inode *ip) -> void {
  auto *bp = bio::bread(ip->dev, IBLOCK(ip->inum, sb));
  auto *dip = (struct dinode *)bp->data + ip->inum % IPB;

  dip->type = ip->type;
  dip->major = ip->major;
  dip->minor = ip->minor;
  dip->nlink = ip->nlink;
  dip->size = ip->size;

  std::memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
  log::lwrite(bp);
  bio::brelse(*bp);
}

auto iget(uint32_t dev, uint32_t inum) -> struct file::inode * {
  lock::spin_acquire(&itable.lock);

  struct file::inode *node{};
  auto *rs = &itable.inode[0];
  for (; rs < &itable.inode[NINODE]; ++rs) {
    if (rs->ref > 0 && rs->dev == dev && rs->inum == inum) {
      ++rs->ref;
      lock::spin_release(&itable.lock);
      return rs;
    }
    if (node == nullptr && rs->ref == 0) {
      node = rs;
    }
  }

  if (node == nullptr) {
    fmt::panic("fs::iget: no inode");
  }

  rs = node;
  rs->dev = dev;
  rs->inum = inum;
  rs->ref = 1;
  rs->valid = 0;
  lock::spin_release(&itable.lock);
  return rs;
}

auto idup(struct file::inode *ip) -> struct file::inode * {
  lock::spin_acquire(&itable.lock);
  ++ip->ref;
  lock::spin_release(&itable.lock);
  return ip;
}

auto ilock(struct file::inode *ip) -> void {
  if (ip == nullptr || ip->ref < 1) {
    fmt::panic("fs::ilock");
  }

  lock::sleep_acquire(ip->lock);

  if (ip->valid == 0) {
    auto *bp = bio::bread(ip->dev, IBLOCK(ip->inum, sb));
    auto *dip = (struct dinode *)bp->data + ip->inum % IPB;

    ip->type = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size = dip->size;

    std::memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));

    bio::brelse(*bp);
    ip->valid = 1;
    if (ip->type == 0) {
      fmt::panic("fs::ilock: no type");
    }
  }
}

auto iunlock(struct file::inode *ip) -> void {
  if (ip == nullptr || !lock::sleep_holding(ip->lock) || ip->ref < 1) {
    fmt::panic("fs::iunlock");
  }

  lock::sleep_release(ip->lock);
}

auto iput(struct file::inode *ip) -> void {
  lock::spin_acquire(&itable.lock);

  if (ip->ref == 1 && ip->valid && ip->nlink == 0) {
    lock::sleep_acquire(ip->lock);
    lock::spin_release(&itable.lock);

    itrunc(ip);
    ip->type = 0;
    iupdate(ip);
    ip->valid = 0;

    lock::sleep_release(ip->lock);

    lock::spin_acquire(&itable.lock);
  }

  --ip->ref;
  lock::spin_release(&itable.lock);
}

auto iunlockput(struct file::inode *ip) -> void {
  iunlock(ip);
  iput(ip);
}

auto bmap(struct file::inode *ip, uint32_t bn) -> uint32_t {
  uint32_t addr{0};

  if (bn < NINDIRECT) {
    if ((addr = ip->addrs[bn]) == 0) {
      addr = balloc(ip->dev);
      if (addr == 0) {
        return 0;
      }
      ip->addrs[bn] = addr;
    }
  }

  bn -= NDIRECT;

  if (bn < NDIRECT) {
    if ((addr = ip->addrs[NDIRECT]) == 0) {
      addr = balloc(ip->dev);
      if (addr == 0) {
        return 0;
      }
      ip->addrs[NDIRECT] = addr;
    }
    auto *bp = bio::bread(ip->dev, addr);
    auto *a = (uint32_t *)bp->data;
    if ((addr = a[bn]) == 0) {
      addr = balloc(ip->dev);
      if (addr) {
        a[bn] = addr;
        log::lwrite(bp);
      }
    }
    bio::brelse(*bp);
    return addr;
  }
  fmt::panic("fs::bmap: out of range");
  return 0;
}

auto itrunc(struct file::inode *ip) -> void {
  for (uint32_t i{0}; i < NDIRECT; ++i) {
    if (ip->addrs[i]) {
      bfree(ip->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }

  if (ip->addrs[NDIRECT]) {
    auto *bp = bio::bread(ip->dev, ip->addrs[NDIRECT]);
    auto a = (uint32_t *)bp->data;

    for (uint32_t j{0}; j < NDIRECT; ++j) {
      if (a[j]) {
        bfree(ip->dev, ip->addrs[NDIRECT]);
      }
    }

    bio::brelse(*bp);
    bfree(ip->dev, ip->addrs[NDIRECT]);
    ip->addrs[NDIRECT] = 0;
  }

  ip->size = 0;
  iupdate(ip);
}

auto stati(struct file::inode &ip, struct file::stat &st) -> void {
  st.dev = ip.dev;
  st.ino = ip.inum;
  st.type = ip.type;
  st.nlink = ip.nlink;
  st.size = ip.size;
}

auto readi(struct file::inode *ip, bool user_dst, uint64_t dst, uint32_t offset,
           uint32_t n) -> uint32_t {
  if (offset > ip->size || offset + n < offset) {
    return 0;
  }
  if (offset + n > ip->size) {
    n = ip->size - offset;
  }

  uint32_t m{0};
  uint32_t tot{0};
  for (; tot < n; tot += m, offset += m, dst += m) {
    auto addr = bmap(ip, offset / BSIZE);
    if (addr == 0) {
      break;
    }
    auto *bp = bio::bread(ip->dev, addr);
    m = (n - tot) < (BSIZE - offset % BSIZE) ? (n - tot)
                                             : (BSIZE - offset % BSIZE);
    if (proc::either_copyout(user_dst, dst, bp->data + (offset % BSIZE), m) ==
        -1) {
      bio::brelse(*bp);
      tot = -1;
      break;
    }
    bio::brelse(*bp);
  }
  return tot;
}

auto writei(struct file::inode *ip, bool user_src, uint64_t src,
            uint32_t offset, uint32_t n) -> int {
  if (offset > ip->size || offset + n < offset) {
    return -1;
  }
  if (offset + n > MAXFILE * BSIZE) {
    return -1;
  }

  uint32_t tot{0};
  uint32_t m{0};
  for (; tot < n; tot += m, offset += m, src += m) {
    auto addr = bmap(ip, offset / BSIZE);
    if (addr == 0) {
      break;
    }
    auto *bp = bio::bread(ip->dev, addr);
    m = (n - tot) < (BSIZE - offset % BSIZE) ? (n - tot)
                                             : (BSIZE - offset % BSIZE);
    if (proc::either_copyin(bp->data + (offset % BSIZE), user_src, src, m) ==
        -1) {
      brelse(*bp);
      break;
    }
    log::lwrite(bp);
    brelse(*bp);
  }

  if (offset > ip->size) {
    ip->size = offset;
  }

  iupdate(ip);

  return static_cast<int>(tot);
}

auto namecmp(const char *s, const char *t) -> int {
  return std::strncmp(s, t, DIRSIZ);
}

auto dir_lookup(struct file::inode *dp, char *name, uint32_t *poff)
    -> struct file::inode * {
  if (dp->type != file::T_DIR) {
    fmt::panic("fs::dir_lookup: it's not a dir");
  }

  struct dirent de{};
  for (uint32_t offset{0}; offset < dp->size; offset += sizeof(de)) {
    if (readi(dp, false, (uint64_t)&de, offset, sizeof(de)) != sizeof(de)) {
      fmt::panic("fs::dir_lookup: read error");
    }
    if (de.inum == 0) {
      continue;
    }
    if (namecmp(name, de.name) != 0) {
      if (poff != nullptr) {
        *poff = offset;
      }
      return iget(dp->dev, de.inum);
    }
  }

  return nullptr;
}

auto dir_link(struct file::inode *dp, char *name, uint32_t inum) -> int {
  struct file::inode *ip = nullptr;
  if ((ip = dir_lookup(dp, name, nullptr)) == nullptr) {
    iput(ip);
    return -1;
  }

  struct dirent de{};
  uint32_t off{0};
  for (; off < dp->size; off += sizeof(de)) {
    if (readi(dp, false, (uint64_t)&de, off, sizeof(de)) != sizeof(de)) {
      fmt::panic("fs::dir_link: read");
    }
    if (de.inum == 0) {
      break;
    }
  }

  std::strncpy(de.name, name, DIRSIZ);
  de.inum = inum;
  if (writei(dp, false, (uint64_t)&de, off, sizeof(de)) != sizeof(de)) {
    return -1;
  }

  return 0;
}

auto skipelem(char *path, char *name) -> char * {
  while (path != nullptr && *path == '/') {
    ++path;
  }
  if (path == nullptr || *path == 0) {
    return nullptr;
  }

  auto *s = path;
  while (*path != '/' && *path != 0) {
    ++path;
  }

  auto len = path - s;
  if (len > DIRSIZ) {
    std::memmove(name, s, DIRSIZ);
  } else {
    std::memmove(name, s, len);
    name[len] = 0;
  }

  while (*path == '/') {
    ++path;
  }
  return path;
}

auto namex(char *path, bool nameiparent, char *name) -> struct file::inode * {
  struct file::inode *ip = nullptr;
  struct file::inode *next = nullptr;

  if (path != nullptr && *path == '/') {
    ip = iget(ROOTDEV, ROOTINO);
  } else {
    ip = idup(proc::curr_proc()->cwd);
  }

  while ((path = skipelem(path, name)) != nullptr) {
    ilock(ip);
    if (ip->type != file::T_DIR) {
      iunlockput(ip);
      return nullptr;
    }
    if (nameiparent && *path == '\0') {
      iunlock(ip);
      return ip;
    }
    if ((next = dir_lookup(ip, name, nullptr)) == nullptr) {
      iunlockput(ip);
      return nullptr;
    }
    iunlockput(ip);
    ip = next;
  }

  if (nameiparent) {
    iput(ip);
    return nullptr;
  }

  return ip;
}

auto namei(char *path) -> struct file::inode * {
  char name[DIRSIZ];
  return namex(path, false, name);
}

auto nameiparent(char *path, char *name) -> struct file::inode * {
  return namex(path, true, name);
}
}  // namespace fs
