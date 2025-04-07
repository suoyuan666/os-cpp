#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ios>
#include <iostream>
#include <string_view>
#include <vector>

#include "kernel/fs"

#define NINODES 200

// Disk layout:
// [ boot block | sb block | log | inode blocks | free bit map | data blocks ]

constexpr uint32_t nbitmap = fs::FSSIZE / fs::BPB + 1;
constexpr uint32_t ninodeblocks = NINODES / fs::IPB + 1;
constexpr uint32_t nlog = fs::LOGSIZE;

struct fs::superblock sb{};
const char zeroes[fs::BSIZE]{};
uint32_t freeinode = 1;
uint32_t freeblock;

void wsect(std::fstream &fd, uint32_t sec, void *buf);
void rsect(std::fstream &fd, uint32_t sec, void *buf);
void winode(std::fstream &fd, uint inum, struct fs::dinode *ip);
void rinode(std::fstream &fd, uint inum, struct fs::dinode *ip);
auto ialloc(std::fstream &fd, ushort type) -> uint32_t;
void balloc(std::fstream &fd, int used);
void iappend(std::fstream &fd, uint inum, void *xp, uint64_t n);
void file_apped(std::fstream &fs, std::string_view &bin_name, uint32_t ino);

auto xshort(ushort x) -> ushort {
  ushort y = 0;
  auto *a = (unsigned char *)&y;
  a[0] = x;
  a[1] = x >> 8U;
  return y;
}

auto xint(uint x) -> uint {
  uint y = 0;
  auto *a = (unsigned char *)&y;
  a[0] = x;
  a[1] = x >> 8U;
  a[2] = x >> 16U;
  a[3] = x >> 24U;
  return y;
}

auto main(int argc, char *argv[]) -> int {
  if (argc < 3) {
    std::cerr << "usage: mkfs fs.img --txt [file] --bin [file] ...\n";
    return -1;
  }

  std::vector<std::string_view> txt_vec{};
  std::vector<std::string_view> bin_vec{};
  auto fetch_txt{false};
  auto fetch_bin{false};

  for (int i = 1; i < argc; ++i) {
    std::string_view argu{argv[i]};
    if (argu == "--txt") {
      fetch_txt = true;
      fetch_bin = false;
      continue;
    }
    if (argu == "--bin") {
      fetch_bin = true;
      fetch_txt = false;
      continue;
    }

    if (fetch_txt) {
      txt_vec.emplace_back(argu);
    } else if (fetch_bin) {
      bin_vec.emplace_back(argu);
    }
  }

  assert(!bin_vec.empty());

  static_assert(fs::BSIZE % sizeof(struct fs::dinode) == 0);
  static_assert((fs::BSIZE % sizeof(struct fs::dirent)) == 0);

  std::fstream fs{"fs.img", std::ios_base::binary | std::ios_base::trunc |
                                std::ios_base::out | std::ios_base::in};

  auto nmeta = 2 + nlog + ninodeblocks + nbitmap;
  auto nblocks = fs::FSSIZE - nmeta;

  sb.magic = fs::FSMAGIC;
  sb.size = xint(fs::FSSIZE);
  sb.nblocks = xint(nblocks);
  sb.ninodes = xint(NINODES);
  sb.nlog = xint(nlog);
  sb.logstart = xint(2);
  sb.inodestart = xint(2 + nlog);
  sb.bmapstart = xint(2 + nlog + ninodeblocks);

  std::cout << "nmeta " << nmeta << " (boot, super, log blocks " << nlog
            << " inode blocks " << ninodeblocks << " bitmap blocks " << nbitmap
            << ") blocks " << nblocks << "  total " << fs::FSSIZE << "\n";

  freeblock = nmeta;

  for (uint32_t i = 0; i < fs::FSSIZE; ++i) {
    wsect(fs, i, (char *)zeroes);
  }

  char buf[fs::BSIZE]{};
  std::memset(buf, 0, fs::BSIZE);
  std::memmove(buf, &sb, sizeof(sb));
  wsect(fs, 1, buf);

  auto rootino = ialloc(fs, fs::T_DIR);
  assert(rootino == 1);

  {
    struct fs::dirent de{};
    de.inum = xshort(rootino);
    de.uid = xint(0);
    de.gid = xint(0);
    de.mask_user = static_cast<char>(6);
    de.mask_group = static_cast<char>(6);
    de.mask_other = static_cast<char>(2);
    strcpy(de.name, ".");
    iappend(fs, rootino, &de, sizeof(de));
  }
  {
    struct fs::dirent de{};
    de.inum = xshort(rootino);
    de.uid = xint(0);
    de.gid = xint(0);
    de.mask_user = static_cast<char>(7);
    de.mask_group = static_cast<char>(7);
    de.mask_other = static_cast<char>(2);
    strcpy(de.name, "..");
    iappend(fs, rootino, &de, sizeof(de));
  }

  constexpr std::array<std::string_view, 3> root_dir_name{"bin", "home", "tmp"};
  std::array<uint32_t, 3> root_dir_id{};
  for (uint32_t i = 0; i < 3; ++i) {
    root_dir_id.at(i) = ialloc(fs, fs::T_DIR);
    struct fs::dirent de{};
    de.inum = xshort(root_dir_id.at(i));
    de.uid = xint(0);
    de.gid = xint(0);
    de.mask_user = static_cast<char>(7);
    de.mask_group = static_cast<char>(7);
    de.mask_other = static_cast<char>(2);
    strcpy(de.name, root_dir_name.at(i).begin());
    iappend(fs, rootino, &de, sizeof(de));
  }

  std::ranges::for_each(
      txt_vec, [&fs](std::string_view &str) { return file_apped(fs, str, 1); });
  auto bin_id = root_dir_id.at(0);
  std::ranges::for_each(bin_vec, [&fs, &bin_id](std::string_view &str) {
    return file_apped(fs, str, bin_id);
  });

  struct fs::dinode din{};
  rinode(fs, rootino, &din);
  auto off = xint(din.size);
  off = ((off / fs::BSIZE) + 1) * fs::BSIZE;
  din.size = xint(off);
  winode(fs, rootino, &din);

  balloc(fs, freeblock);

  return 0;
}

void wsect(std::fstream &fd, uint32_t sec, void *buf) {
  if (!fd.seekp(static_cast<int>(sec * fs::BSIZE), std::ios_base::beg)) {
    std::cerr << "wsect: fd.seekg error\n";
    exit(-1);
  }
  if (!fd.write(static_cast<char *>(buf), fs::BSIZE)) {
    std::cerr << "wsect: fd.write error\n";
    exit(-1);
  }
  if (!fd.seekg(std::ios_base::beg)) {
    std::cerr << "wsect:: fd.seekg error\n";
    exit(-1);
  }
}

void rsect(std::fstream &fd, uint32_t sec, void *buf) {
  if (!fd.seekp(static_cast<int>(sec * fs::BSIZE), std::ios_base::beg)) {
    std::cerr << "rsect: fd.seekp error\n";
    exit(-1);
  }
  if (!fd.read(static_cast<char *>(buf), fs::BSIZE)) {
    std::cerr << "rsect: fd.read error\n";
    exit(-1);
  }
  if (!fd.seekp(std::ios_base::beg)) {
    std::cerr << "rsect:: fd.seekp error\n";
    exit(-1);
  }
}

void winode(std::fstream &fd, uint inum, struct fs::dinode *ip) {
  char buf[fs::BSIZE];
  uint32_t bn = 0;
  struct fs::dinode *dip = nullptr;

  bn = ((inum) / fs::IPB + sb.inodestart);
  rsect(fd, bn, buf);
  dip = ((struct fs::dinode *)buf) + (inum % fs::IPB);
  *dip = *ip;
  wsect(fd, bn, buf);
}

void rinode(std::fstream &fd, uint inum, struct fs::dinode *ip) {
  char buf[fs::BSIZE];
  uint32_t bn = 0;
  struct fs::dinode *dip = nullptr;

  bn = ((inum) / fs::IPB + sb.inodestart);
  rsect(fd, bn, buf);
  dip = ((struct fs::dinode *)buf) + (inum % fs::IPB);
  *ip = *dip;
}

auto ialloc(std::fstream &fd, ushort type) -> uint32_t {
  uint inum = freeinode++;
  struct fs::dinode din{};

  std::memset(&din, '\0', sizeof(din));
  din.type = static_cast<int16_t>(xshort(type));
  din.nlink = static_cast<int16_t>(xshort(1));
  din.size = xint(0);
  winode(fd, inum, &din);
  return inum;
}

void balloc(std::fstream &fd, int used) {
  unsigned char buf[fs::BSIZE];

  std::cout << "balloc: first " << used << " blocks have been allocated\n";
  assert(used < static_cast<int>(fs::BPB));
  bzero(buf, fs::BSIZE);
  for (int i = 0; i < used; i++) {
    buf[i / 8] = buf[i / 8] | (0x1 << (i % 8));
  }
  std::cout << "balloc: write bitmap block at sector " << sb.bmapstart << "\n";
  wsect(fd, sb.bmapstart, buf);
}

void iappend(std::fstream &fd, uint inum, void *xp, uint64_t n) {
  char *p = static_cast<char *>(xp);
  uint32_t fbn = 0, off = 0, n1 = 0;
  struct fs::dinode din{};
  char buf[fs::BSIZE];
  uint indirect[fs::NINDIRECT];
  uint x = 0;

  rinode(fd, inum, &din);
  off = xint(din.size);
  while (n > 0) {
    fbn = off / fs::BSIZE;
    assert(fbn < fs::MAXFILE);
    if (fbn < fs::NDIRECT) {
      if (xint(din.addrs[fbn]) == 0) {
        din.addrs[fbn] = xint(freeblock++);
      }
      x = xint(din.addrs[fbn]);
    } else {
      if (xint(din.addrs[fs::NDIRECT]) == 0) {
        din.addrs[fs::NDIRECT] = xint(freeblock++);
      }
      rsect(fd, xint(din.addrs[fs::NDIRECT]), (char *)indirect);
      if (indirect[fbn - fs::NDIRECT] == 0) {
        indirect[fbn - fs::NDIRECT] = xint(freeblock++);
        wsect(fd, xint(din.addrs[fs::NDIRECT]), (char *)indirect);
      }
      x = xint(indirect[fbn - fs::NDIRECT]);
    }
    n1 = std::min(static_cast<uint32_t>(n), (fbn + 1) * fs::BSIZE - off);
    rsect(fd, x, buf);
    bcopy(p, buf + off - (fbn * fs::BSIZE), n1);
    wsect(fd, x, buf);

    n -= n1;
    off += n1;
    p += n1;
  }
  din.size = xint(off);
  winode(fd, inum, &din);
}

void file_apped(std::fstream &fs, std::string_view &bin_name, uint32_t ino) {
  const auto *shortname = bin_name.begin();
  if (ino != 1) {  // rootino == 1, strlen("build/utils/") == 12
    shortname = bin_name.begin() + 12;
  }

  assert(index(shortname, '/') == nullptr);

  std::cout << "[LOG]: append the file to rootfs: " << shortname << "\n";

  std::ifstream fd{bin_name.begin(), std::ios_base::binary};

  assert(strlen(shortname) <= fs::DIRSIZ);

  auto inum = ialloc(fs, fs::T_FILE);

  struct fs::dirent de{};
  std::memset(&de, 0, sizeof(de));
  de.inum = xshort(inum);
  de.uid = 1000;
  de.gid = 1000;
  de.mask_user = static_cast<char>(6);
  de.mask_group = static_cast<char>(6);
  de.mask_other = static_cast<char>(2);
  strncpy(de.name, shortname, fs::DIRSIZ);
  iappend(fs, ino, &de, sizeof(de));

  char buf[fs::BSIZE]{};
  while (true) {
    fd.read(buf, sizeof buf);
    auto size = static_cast<uint64_t>(fd.gcount());
    if (size == 0) {
      break;
    }
    iappend(fs, inum, buf, size);
  }
}
