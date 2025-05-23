#pragma once
#include <array>
#include <cstdint>

#include "kernel/fs"
#include "lock.h"

namespace file {
struct file {
  enum : uint8_t { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type;
  int ref;
  bool readable;
  bool writable;
  struct pipe* pipe;
  struct inode* ip;
  uint32_t off;
  int16_t major;
};

static inline auto major(uint32_t dev) -> uint32_t {
  return (dev) >> 16U & 0xFFFFU;
}
static inline auto minor(uint32_t dev) -> uint32_t { return dev & 0xFFFFU; }
static inline auto mkdev(uint32_t m, uint32_t n) -> uint32_t {
  return m << 16U | n;
}

struct inode {
  uint32_t dev;
  uint32_t inum;
  int ref;
  int valid;
  uint32_t uid;
  uint32_t gid;
  unsigned char mask_user;
  unsigned char mask_group;
  unsigned char mask_other;

  int16_t type;
  int16_t major;
  int16_t minor;
  int16_t nlink;
  uint32_t size;
  uint32_t addrs[fs::NDIRECT + 1];
  class lock::sleeplock lock{};
};

struct devsw {
  int (*read)(bool, uint64_t, int);
  int (*write)(bool, uint64_t, int);
};

extern struct devsw devsw[];

constexpr uint8_t CONSOLE{1};

constexpr uint32_t T_DIR{1};
constexpr uint32_t T_FILE{2};
constexpr uint32_t T_DEVICE{3};

struct stat {
  uint32_t dev;
  uint32_t ino;
  int16_t type;
  int16_t nlink;
  uint64_t size;
  uint32_t uid;
  uint32_t gid;
};

constexpr uint32_t NOFILE{16};
constexpr uint32_t NFILE{100};
constexpr uint32_t NDEV{10};

constexpr uint32_t MAXPATH{128};

constexpr uint32_t O_RDONLY{0x000};
constexpr uint32_t O_WRONLY{0x001};
constexpr uint32_t O_RDWR{0x002};
constexpr uint32_t O_CREATE{0x200};
constexpr uint32_t O_TRUNC{0x400};

auto alloc() -> struct file*;
auto dup(struct file* f) -> struct file*;
auto close(struct file* f) -> void;
auto stat(struct file* f, uint64_t addr) -> int;
auto read(struct file* f, uint64_t addr, int n) -> int;
auto write(struct file* f, uint64_t addr, int n) -> int;
}  // namespace file
