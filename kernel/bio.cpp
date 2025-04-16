#include "bio.h"

#include <array>
#include <cstdint>
#include <fmt>

#include "kernel/fs"
#include "lock.h"
#include "virtio.h"

namespace bio {

class bcache {
 public:
  class buf head{};
  class lock::spinlock lock{};
  std::array<buf, fs::NBUF> buf{};
} bcache{};

auto init() -> void {
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;

  for (auto &b : bcache.buf) {
    b.next = bcache.head.next;
    b.prev = &bcache.head;
    bcache.head.next->prev = &b;
    bcache.head.next = &b;
  }
}

auto bget(uint32_t dev, uint32_t blockno) -> class buf * {
  bcache.lock.acquire();

  // if already cached
  auto *rs = bcache.head.next;
  while (rs != &bcache.head) {
    if (rs->dev == dev && rs->blockno == blockno) {
      ++rs->refcnt;
      bcache.lock.release();
      rs->lock.acquire();
      return rs;
    }
    rs = rs->next;
  }

  // no!!! no cache
  rs = bcache.head.next;
  while (rs != &bcache.head) {
    if (rs->refcnt == 0) {
      rs->dev = dev;
      rs->blockno = blockno;
      rs->valid = 0;
      rs->refcnt = 1;
      bcache.lock.release();
      rs->lock.acquire();
      return rs;
    }
    rs = rs->next;
  }

  fmt::panic("no buffers");
  return rs;
}

auto bread(uint32_t dev, uint32_t blockno) -> class buf * {
  auto *rs = bget(dev, blockno);
  if (!rs->valid) {
    virtio::disk_rw(rs, false);
    rs->valid = 1;
  }
  return rs;
}

auto brelse(class buf &b) -> void {
  if (!b.lock.holding()) {
    fmt::panic("bio::brelse: no lock");
  }
  b.lock.release();

  bcache.lock.acquire();
  --b.refcnt;
  if (b.refcnt == 0) {
    b.next->prev = b.prev;
    b.prev->next = b.next;
    b.next = bcache.head.next;
    b.prev = &bcache.head;
    bcache.head.next->prev = &b;
    bcache.head.next = &b;
  }
  bcache.lock.release();
}

auto bwrite(class buf *buf) -> void {
  if (!buf->lock.holding()) {
    fmt::panic("bio::bwrite: no lock");
  }
  virtio::disk_rw(buf, true);
}

auto bpin(class buf *buf) -> void {
  bcache.lock.acquire();
  ++buf->refcnt;
  bcache.lock.release();
}

auto bupin(class buf *buf) -> void {
  bcache.lock.acquire();
  --buf->refcnt;
  bcache.lock.release();
}
}  // namespace bio
