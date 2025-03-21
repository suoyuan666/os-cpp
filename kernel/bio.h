#pragma once
#include <cstdint>

#include "kernel/fs"
#include "lock.h"

namespace bio {
class buf {
 public:
  int valid{0};  // has data been read from disk?
  int disk{0};   // does disk "own" buf?
  uint32_t dev{0};
  uint32_t blockno{0};
  uint32_t refcnt{0};
  class lock::sleeplock lock{};
  class buf *prev{nullptr};  // LRU cache list
  class buf *next{nullptr};
  unsigned char data[fs::BSIZE]{0};
};

auto init() -> void;
auto bread(uint32_t dev, uint32_t blockno) -> class buf *;
auto bget(uint32_t dev, uint32_t blockno) -> class buf *;
auto brelse(class buf &b) -> void;
auto bwrite(class buf *buf) -> void;
auto bpin(class buf *buf) -> void;
auto bupin(class buf *buf) -> void;
}  // namespace bio
