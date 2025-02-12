#include "bio"

#include <cstdint>

#include "fmt"
#include "lock"
#include "virtio_disk"

namespace bio {

struct bcache {
  struct lock::spinlock lock;
  struct buf buf[fs::NBUF];
  struct buf head;
} bcache;

auto init() -> void {
  lock::spin_init(bcache.lock, (char *)"bcache");
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;

  for (auto *b = bcache.buf; b < bcache.buf + fs::NBUF; ++b) {
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    lock::sleep_init(b->lock, (char *)"buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}

auto bget(uint32_t dev, uint32_t blockno) -> struct buf * {
  // if already cached
  struct buf *rs{};

  lock::spin_acquire(&bcache.lock);

  for (rs = bcache.head.next; rs != &bcache.head; rs = rs->next) {
    if (rs->dev == dev && rs->blockno == blockno) {
      ++rs->refcnt;
      lock::spin_release(&bcache.lock);
      lock::sleep_acquire(rs->lock);
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
      lock::spin_release(&bcache.lock);
      lock::sleep_acquire(rs->lock);
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
  if (lock::sleep_holding(b.lock) == false) {
    fmt::panic("bio::brelse: no lock");
  }
  lock::sleep_release(b.lock);

  lock::spin_acquire(&bcache.lock);
  --b.refcnt;
  if (b.refcnt == 0) {
    b.next->prev = b.prev;
    b.prev->next = b.next;
    b.next = bcache.head.next;
    b.prev = &bcache.head;
    bcache.head.next->prev = &b;
    bcache.head.next = &b;
  }
  lock::spin_release(&bcache.lock);
}

auto bwrite(struct buf *buf) -> void {
  if (lock::sleep_holding(buf->lock) == false) {
    fmt::panic("bio::bwrite: no lock");
  }
  virtio_disk::disk_rw(buf, true);
}

auto bpin(struct buf *buf) -> void {
  lock::sleep_acquire(buf->lock);
  ++buf->refcnt;
  lock::sleep_release(buf->lock);
}

auto bupin(struct buf *buf) -> void {
  lock::sleep_acquire(buf->lock);
  --buf->refcnt;
  lock::sleep_release(buf->lock);
}
}  // namespace bio
