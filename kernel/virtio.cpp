#include "virtio.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <fmt>

#include "bio.h"
#include "lock.h"
#include "proc.h"
#include "vm.h"

namespace virtio {
static inline auto R(uint64_t r) -> volatile uint32_t * {
  return reinterpret_cast<uint32_t *>(vm::VIRTIO0 + r);
};

struct disk_info {
  class bio::buf *b;
  char status;
};

static struct disk {
  uint16_t used_idx{};
  struct virtq_desc *desc{};
  struct virtq_avail *avail{};
  struct virtq_used *used{};

  std::array<char, NUM> free{};
  std::array<struct disk_info, NUM> info{};
  std::array<struct virtio_blk_req, NUM> ops{};

  class lock::spinlock vdisk_lock{};
} disk;

auto init() -> void {
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

  auto max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
  if (max == 0) {
    fmt::panic("virtio disk has no queue 0");
  }
  if (max < NUM) {
    fmt::panic("virtio disk max queue too short");
  }

  auto opt_desc = vm::kalloc();
  if (!opt_desc.has_value()) {
    fmt::panic("virtio_disk: vm::kalloc() has no memory");
  }
  disk.desc = reinterpret_cast<struct virtq_desc *>(opt_desc.value());

  auto opt_avail = vm::kalloc();
  if (!opt_avail.has_value()) {
    fmt::panic("virtio_disk: vm::kalloc() has no memory");
  }
  disk.avail = reinterpret_cast<struct virtq_avail *>(opt_avail.value());

  auto opt_used = vm::kalloc();
  if (!opt_used.has_value()) {
    fmt::panic("virtio_disk: vm::kalloc() has no memory");
  }
  disk.used = reinterpret_cast<struct virtq_used *>(opt_used.value());
  if (!disk.desc || !disk.avail || !disk.used) {
    fmt::panic("virtio disk vm::alloc");
  }

  std::memset(disk.desc, 0, PGSIZE);
  std::memset(disk.avail, 0, PGSIZE);
  std::memset(disk.used, 0, PGSIZE);

  *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;

  *R(VIRTIO_MMIO_QUEUE_DESC_LOW) = reinterpret_cast<uint64_t>(disk.desc);
  *R(VIRTIO_MMIO_QUEUE_DESC_HIGH) =
      reinterpret_cast<uint64_t>(disk.desc) >> 32U;
  *R(VIRTIO_MMIO_DRIVER_DESC_LOW) = reinterpret_cast<uint64_t>(disk.avail);
  *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) =
      reinterpret_cast<uint64_t>(disk.avail) >> 32U;
  *R(VIRTIO_MMIO_DEVICE_DESC_LOW) = reinterpret_cast<uint64_t>(disk.used);
  *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) =
      reinterpret_cast<uint64_t>(disk.used) >> 32U;

  *R(VIRTIO_MMIO_QUEUE_READY) = 0x1;

  for (auto &i : disk.free) {
    i = 1;
  }

  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  *R(VIRTIO_MMIO_STATUS) = status;
}

auto alloc_desc() -> int {
  for (int i{0}; i < static_cast<int>(NUM); ++i) {
    if (disk.free.at(i)) {
      disk.free.at(i) = 0;
      return i;
    }
  }
  return -1;
}

auto free_desc(int i) -> void {
  if (i >= static_cast<int>(NUM)) {
    fmt::panic("free_desc: 1");
  }
  if (disk.free.at(i)) {
    fmt::panic("free_desc: 2");
  }

  disk.desc[i].addr = 0;
  disk.desc[i].len = 0;
  disk.desc[i].flags = 0;
  disk.desc[i].next = 0;
  disk.free.at(i) = 1;
}

auto alloc3_desc(std::array<int, 3> &idx) -> int {
  for (auto i{0}; i < 3; ++i) {
    idx.at(i) = alloc_desc();
    if (idx.at(i) < 0) {
      for (auto j{0}; j < i; ++j) {
        free_desc(idx.at(j));
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

auto disk_rw(class bio::buf *b, bool write) -> void {
  auto sector = static_cast<uint64_t>(b->blockno * (fs::BSIZE / 512));

  disk.vdisk_lock.acquire();

  std::array<int, 3> idx{};
  while (true) {
    if (alloc3_desc(idx) == 0) {
      break;
    }
    proc::sleep(&disk.free[0], disk.vdisk_lock);
  }

  auto *buf0 = &disk.ops.at(idx.at(0));

  if (write) {
    buf0->type = VIRTIO_BLK_T_OUT;
  } else {
    buf0->type = VIRTIO_BLK_T_IN;
  }
  buf0->reserved = 0;
  buf0->sector = sector;

  disk.desc[idx.at(0)].addr = reinterpret_cast<uint64_t>(buf0);
  disk.desc[idx.at(0)].len = sizeof(struct virtio_blk_req);
  disk.desc[idx.at(0)].flags = VRING_DESC_F_NEXT;
  disk.desc[idx.at(0)].next = idx.at(1);

  disk.desc[idx.at(1)].addr = reinterpret_cast<uint64_t>(b->data.begin());
  disk.desc[idx.at(1)].len = fs::BSIZE;
  if (write) {
    disk.desc[idx.at(1)].flags = 0;
  } else {
    disk.desc[idx.at(1)].flags = VRING_DESC_F_WRITE;
  }
  disk.desc[idx.at(1)].flags |= VRING_DESC_F_NEXT;
  disk.desc[idx.at(1)].next = idx.at(2);

  disk.info.at(idx.at(0)).status = 0xff;
  disk.desc[idx.at(2)].addr =
      reinterpret_cast<uint64_t>(&disk.info.at(idx.at(0)).status);
  disk.desc[idx.at(2)].len = 1;
  disk.desc[idx.at(2)].flags = VRING_DESC_F_WRITE;
  disk.desc[idx.at(2)].next = 0;

  b->disk = 1;
  disk.info.at(idx.at(0)).b = b;

  disk.avail->ring.at(disk.avail->idx % NUM) = idx.at(0);

  __sync_synchronize();

  disk.avail->idx += 1;

  __sync_synchronize();

  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0;

  while (b->disk == 1) {
    proc::sleep(b, disk.vdisk_lock);
  }

  disk.info.at(idx.at(0)).b = nullptr;
  free_chain(idx.at(0));
  disk.vdisk_lock.release();
}

auto virtio_disk_intr() -> void {
  disk.vdisk_lock.acquire();

  *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3U;

  __sync_synchronize();

  while (disk.used_idx != disk.used->idx) {
    __sync_synchronize();
    auto id = disk.used->ring.at(disk.used_idx % NUM).id;

    if (disk.info.at(id).status != 0) {
      fmt::panic("virtio_disk_intr status");
    }

    auto *b = disk.info.at(id).b;
    b->disk = 0;
    proc::wakeup(b);

    disk.used_idx += 1;
  }

  disk.vdisk_lock.release();
}
}  // namespace virtio
