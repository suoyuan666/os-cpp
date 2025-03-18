#include "bio"

#include <cstdint>

#include "fmt"
#include "lock"
#include "virtio_disk"

namespace bio {

struct bcache {
  class lock::spinlock lock{"bcache"};
  struct buf buf[fs::NBUF];
  struct buf head;
} bcache;

auto init() -> void {
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;

  for (auto *b = bcache.buf; b < bcache.buf + fs::NBUF; ++b) {
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}

auto bget(uint32_t dev, uint32_t blockno) -> struct buf * {
  // if already cached
  struct buf *rs{};

  bcache.lock.acquire();

  for (rs = bcache.head.next; rs != &bcache.head; rs = rs->next) {
    if (rs->dev == dev && rs->blockno == blockno) {
      ++rs->refcnt;
      bcache.lock.release();
      rs->lock.acquire();
      return rs;
    }
  }

  // no!!! no cache
  for (rs = bcache.head.next; rs != &bcache.head; rs = rs->next) {
    if (rs->refcnt == 0) {
      rs->dev = dev;
      rs->blockno = blockno;
      rs->valid = 0;
      rs->refcnt = 1;
      bcache.lock.release();
      rs->lock.acquire();
      return rs;
    }
  }

  fmt::panic("no buffers");
  return rs;
}

auto bread(uint32_t dev, uint32_t blockno) -> struct buf * {
  auto *rs = bget(dev, blockno);
  if (!rs->valid) {
    virtio_disk::disk_rw(rs, false);
    rs->valid = 1;
  }
  return rs;
}

auto brelse(struct buf &b) -> void {
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

auto bwrite(struct buf *buf) -> void {
  if (!buf->lock.holding()) {
    fmt::panic("bio::bwrite: no lock");
  }
  virtio_disk::disk_rw(buf, true);
}

auto bpin(struct buf *buf) -> void {
  bcache.lock.acquire();
  ++buf->refcnt;
  bcache.lock.release();
}

auto bupin(struct buf *buf) -> void {
  bcache.lock.acquire();
  --buf->refcnt;
  bcache.lock.release();
}
}  // namespace bio
