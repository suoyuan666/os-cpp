#include "kernel/fs"

#include <cstdint>
#include <cstring>
#include <fmt>

#include "bio.h"
#include "file.h"
#include "fs.h"
#include "lock.h"
#include "log.h"
#include "proc.h"
#include "virtio_disk.h"

namespace fs {

#define min(a, b) ((a) < (b) ? (a) : (b))

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
  class lock::spinlock lock{};
  struct file::inode inode[fs::NINODE];
} itable;

auto iget(uint32_t dev, uint32_t inum) -> struct file::inode *;

auto ialloc(uint32_t dev, int16_t type) -> struct file::inode * {
  for (uint32_t inum = 1; inum < sb.ninodes; ++inum) {
    auto *bp = bio::bread(dev, IBLOCK(inum, sb));
    auto *dip = (struct dinode *)bp->data + inum % IPB;
    if (dip->type == 0) {
      std::memset(dip, 0, sizeof(*dip));
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
  auto *dip = reinterpret_cast<struct dinode *>(bp->data) + ip->inum % IPB;

  dip->type = ip->type;
  dip->major = ip->major;
  dip->minor = ip->minor;
  dip->nlink = ip->nlink;
  dip->size = ip->size;
  dip->uid = ip->uid;
  dip->gid = ip->gid;
  dip->mask_user = ip->mask_user;
  dip->mask_group = ip->mask_group;
  dip->mask_other = ip->mask_other;

  std::memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
  log::lwrite(bp);
  bio::brelse(*bp);
}

auto iget(uint32_t dev, uint32_t inum) -> struct file::inode * {
  itable.lock.acquire();

  struct file::inode *node{};
  auto *rs = &itable.inode[0];
  for (; rs < &itable.inode[NINODE]; ++rs) {
    if (rs->ref > 0 && rs->dev == dev && rs->inum == inum) {
      ++rs->ref;
      itable.lock.release();
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
  itable.lock.release();
  return rs;
}

auto idup(struct file::inode *ip) -> struct file::inode * {
  itable.lock.acquire();
  ++ip->ref;
  itable.lock.release();
  return ip;
}

auto ilock(struct file::inode *ip) -> void {
  if (ip == nullptr || ip->ref < 1) {
    fmt::panic("fs::ilock");
  }

  ip->lock.acquire();

  if (ip->valid == 0) {
    auto *bp = bio::bread(ip->dev, IBLOCK(ip->inum, sb));
    auto *dip = (struct dinode *)bp->data + ip->inum % IPB;

    ip->type = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size = dip->size;
    ip->uid = dip->uid;
    ip->gid = dip->gid;
    ip->mask_user = dip->mask_user;
    ip->mask_group = dip->mask_group;
    ip->mask_other = dip->mask_other;

    std::memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));

    bio::brelse(*bp);
    ip->valid = 1;
    if (ip->type == 0) {
      fmt::panic("fs::ilock: no type");
    }
  }
}

auto iunlock(struct file::inode *ip) -> void {
  if (ip == nullptr || !ip->lock.holding() || ip->ref < 1) {
    fmt::panic("fs::iunlock");
  }

  ip->lock.release();
}

auto iput(struct file::inode *ip) -> void {
  itable.lock.acquire();

  if (ip->ref == 1 && ip->valid && ip->nlink == 0) {
    ip->lock.acquire();
    itable.lock.release();

    itrunc(ip);
    ip->type = 0;
    iupdate(ip);
    ip->valid = 0;

    ip->lock.release();
    itable.lock.acquire();
  }

  --ip->ref;
  itable.lock.release();
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
    return addr;
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
    auto *a = reinterpret_cast<uint32_t *>(bp->data);

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
  st.uid = ip.uid;
  st.gid = ip.gid;
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
    m = min(n - tot, BSIZE - offset % BSIZE);
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
    if (namecmp(name, de.name) == 0) {
      if (poff != nullptr) {
        *poff = offset;
      }
      return iget(dp->dev, de.inum);
    }
  }

  return nullptr;
}

auto dir_link(struct file::inode *dp, char *name, struct file::inode *other)
    -> int {
  struct file::inode *ip = dir_lookup(dp, name, nullptr);
  if (ip != nullptr) {
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
  de.inum = other->inum;
  de.uid = other->uid;
  de.gid = other->gid;
  de.mask_user = other->mask_user;
  de.mask_group = other->mask_group;
  de.mask_other = other->mask_other;
  if (writei(dp, false, (uint64_t)&de, off, sizeof(de)) != sizeof(de)) {
    return -1;
  }

  return 0;
}

auto dir_link(struct file::inode *dp, char *name, uint32_t inum) -> int {
  struct file::inode *ip = dir_lookup(dp, name, nullptr);
  if (ip != nullptr) {
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
    next = dir_lookup(ip, name, nullptr);
    if (next == nullptr) {
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
