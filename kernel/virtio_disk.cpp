#include "virtio_disk"

#include <cstdint>
#include <cstring>

#include "bio"
#include "fmt"
#include "fs"
#include "lock"
#include "proc"
#include "vm"

namespace virtio_disk {
static inline auto R(uint64_t r) -> volatile uint32_t * {
  return (uint32_t *)(vm::VIRTIO0 + r);
};

// #define R(r) ((volatile uint32_t *)(vm::VIRTIO0 + (r)))

static struct disk {
  struct virtq_desc *desc;
  struct virtq_avail *avail;
  struct virtq_used *used;

  char free[NUM];
  uint16_t used_idx;

  struct {
    struct bio::buf *b;
    char status;
  } info[NUM];

  struct virtio_blk_req ops[NUM];

  struct lock::spinlock vdisk_lock;
} disk;

auto init() -> void {
  lock::spin_init(disk.vdisk_lock, (char *)"virtio_disk");

  if (*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
      *R(VIRTIO_MMIO_VERSION) != 2 || *R(VIRTIO_MMIO_DEVICE_ID) != 2 ||
      *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551) {
    fmt::panic("could not find virtio disk");
  }

  uint32_t status{0};
  *R(VIRTIO_MMIO_STATUS) = status;

  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
  *R(VIRTIO_MMIO_STATUS) = status;

  status |= VIRTIO_CONFIG_S_DRIVER;
  *R(VIRTIO_MMIO_STATUS) = status;

  uint64_t features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
  features &= ~(1U << VIRTIO_BLK_F_RO);
  features &= ~(1U << VIRTIO_BLK_F_SCSI);
  features &= ~(1U << VIRTIO_BLK_F_CONFIG_WCE);
  features &= ~(1U << VIRTIO_BLK_F_MQ);
  features &= ~(1U << VIRTIO_F_ANY_LAYOUT);
  features &= ~(1U << VIRTIO_RING_F_EVENT_IDX);
  features &= ~(1U << VIRTIO_RING_F_INDIRECT_DESC);
  *R(VIRTIO_MMIO_DRIVER_FEATURES) = features;

  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  status = *R(VIRTIO_MMIO_STATUS);
  if (!(status & VIRTIO_CONFIG_S_FEATURES_OK)) {
    fmt::panic("virtio disk FEATURES_OK unset");
  }

  *R(VIRTIO_MMIO_QUEUE_SEL) = 0;

  if (*R(VIRTIO_MMIO_QUEUE_READY)) {
    fmt::panic("virtio disk should not be ready");
  }

  uint32_t max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
  if (max == 0) {
    fmt::panic("virtio disk has no queue 0");
  }
  if (max < NUM) {
    fmt::panic("virtio disk max queue too short");
  }

  disk.desc = (struct virtq_desc *)vm::kalloc().value();
  disk.avail = (struct virtq_avail *)vm::kalloc().value();
  disk.used = (struct virtq_used *)vm::kalloc().value();
  if (!disk.desc || !disk.avail || !disk.used) {
    fmt::panic("virtio disk vm::alloc");
  }
  std::memset(disk.desc, 0, PGSIZE);
  std::memset(disk.avail, 0, PGSIZE);
  std::memset(disk.used, 0, PGSIZE);

  *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;

  *R(VIRTIO_MMIO_QUEUE_DESC_LOW) = (uint64_t)disk.desc;
  *R(VIRTIO_MMIO_QUEUE_DESC_HIGH) = (uint64_t)disk.desc >> 32;
  *R(VIRTIO_MMIO_DRIVER_DESC_LOW) = (uint64_t)disk.avail;
  *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64_t)disk.avail >> 32;
  *R(VIRTIO_MMIO_DEVICE_DESC_LOW) = (uint64_t)disk.used;
  *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64_t)disk.used >> 32;

  *R(VIRTIO_MMIO_QUEUE_READY) = 0x1;

  for (char & i : disk.free) {
    i = 1;
  }

  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  *R(VIRTIO_MMIO_STATUS) = status;
}

auto alloc_desc() -> int {
  for (uint32_t i{0}; i < NUM; ++i) {
    if (disk.free[i]) {
      disk.free[i] = 0;
      return i;
    }
  }
  return -1;
}

auto free_desc(int i) -> void {
  if (i >= static_cast<int>(NUM)) {
    fmt::panic("free_desc: 1");
  }
  if (disk.free[i]) {
    fmt::panic("free_desc: 2");
  }

  disk.desc[i].addr = 0;
  disk.desc[i].len = 0;
  disk.desc[i].flags = 0;
  disk.desc[i].next = 0;
  disk.free[i] = 1;
}

auto alloc3_desc(int *idx) -> int {
  for (auto i{0}; i < 3; ++i) {
    idx[i] = alloc_desc();
    if (idx[i] < 0) {
      for (auto j{0}; j < i; ++j) {
        free_desc(idx[j]);
      }
      return -1;
    }
  }
  return 0;
}

auto free_chain(int i) -> void {
  while (true) {
    auto flag = disk.desc[i].flags;
    auto nxt = disk.desc[i].next;
    free_desc(i);
    if (flag & VRING_DESC_F_NEXT) {
      i = nxt;
    } else {
      break;
    }
  }
}

auto disk_rw(struct bio::buf *b, bool write) -> void {
  uint64_t sector = b->blockno * (fs::BSIZE / 512);

  lock::spin_acquire(&disk.vdisk_lock);

  int idx[3];
  while (true) {
    if (alloc3_desc(idx) == 0) {
      break;
    }
    proc::sleep(&disk.free[0], disk.vdisk_lock);
  }

  auto *buf0 = &disk.ops[idx[0]];

  if (write) {
    buf0->type = VIRTIO_BLK_T_OUT;
  } else {
    buf0->type = VIRTIO_BLK_T_IN;
  }
  buf0->reserved = 0;
  buf0->sector = sector;

  disk.desc[idx[0]].addr = (uint64_t)buf0;
  disk.desc[idx[0]].len = sizeof(struct virtio_blk_req);
  disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
  disk.desc[idx[0]].next = idx[1];

  disk.desc[idx[1]].addr = (uint64_t)b->data;
  disk.desc[idx[1]].len = fs::BSIZE;
  if (write)
    disk.desc[idx[1]].flags = 0;
  else
    disk.desc[idx[1]].flags = VRING_DESC_F_WRITE;
  disk.desc[idx[1]].flags |= VRING_DESC_F_NEXT;
  disk.desc[idx[1]].next = idx[2];

  disk.info[idx[0]].status = 0xff;
  disk.desc[idx[2]].addr = (uint64_t)&disk.info[idx[0]].status;
  disk.desc[idx[2]].len = 1;
  disk.desc[idx[2]].flags = VRING_DESC_F_WRITE;
  disk.desc[idx[2]].next = 0;

  b->disk = 1;
  disk.info[idx[0]].b = b;

  disk.avail->ring[disk.avail->idx % NUM] = idx[0];

  __sync_synchronize();

  disk.avail->idx += 1;

  __sync_synchronize();

  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0;

  while (b->disk == 1) {
    proc::sleep(b, disk.vdisk_lock);
  }

  disk.info[idx[0]].b = 0;
  free_chain(idx[0]);
  lock::spin_release(&disk.vdisk_lock);
}

auto virtio_disk_intr() -> void {
  spin_acquire(&disk.vdisk_lock);

  *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3U;

  __sync_synchronize();

  while (disk.used_idx != disk.used->idx) {
    __sync_synchronize();
    int id = disk.used->ring[disk.used_idx % NUM].id;

    if (disk.info[id].status != 0) {
      fmt::panic("virtio_disk_intr status");
    }

    auto *b = disk.info[id].b;
    b->disk = 0;
    proc::wakeup(b);

    disk.used_idx += 1;
  }

  lock::spin_release(&disk.vdisk_lock);
}
}  // namespace virtio_disk
