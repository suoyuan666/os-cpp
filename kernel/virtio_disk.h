#pragma once
#include <cstdint>

#include "bio.h"

namespace virtio_disk {
//
// virtio device definitions.
// for both the mmio interface, and virtio descriptors.
// only tested with qemu.
//
// the virtio spec:
// https://docs.oasis-open.org/virtio/virtio/v1.1/virtio-v1.1.pdf
//

// virtio mmio control registers, mapped starting at 0x10001000.
// from qemu virtio_mmio.h
// clang-format off
constexpr uint32_t VIRTIO_MMIO_MAGIC_VALUE        {0x000};  // 0x74726976
constexpr uint32_t VIRTIO_MMIO_VERSION            {0x004};      // version; should be 2
constexpr uint32_t VIRTIO_MMIO_DEVICE_ID          {0x008};  // device type; 1 is net, 2 is disk
constexpr uint32_t VIRTIO_MMIO_VENDOR_ID          {0x00c};  // 0x554d4551
constexpr uint32_t VIRTIO_MMIO_DEVICE_FEATURES    {0x010};
constexpr uint32_t VIRTIO_MMIO_DRIVER_FEATURES    {0x020};
constexpr uint32_t VIRTIO_MMIO_QUEUE_SEL          {0x030};  // select queue, write-only
constexpr uint32_t VIRTIO_MMIO_QUEUE_NUM_MAX      {0x034};  // max size of current queue, read-only
constexpr uint32_t VIRTIO_MMIO_QUEUE_NUM          {0x038};  // size of current queue, write-only
constexpr uint32_t VIRTIO_MMIO_QUEUE_READY        {0x044};       // ready bit
constexpr uint32_t VIRTIO_MMIO_QUEUE_NOTIFY       {0x050};      // write-only
constexpr uint32_t VIRTIO_MMIO_INTERRUPT_STATUS   {0x060};  // read-only
constexpr uint32_t VIRTIO_MMIO_INTERRUPT_ACK      {0x064};     // write-only
constexpr uint32_t VIRTIO_MMIO_STATUS             {0x070};            // read/write
constexpr uint32_t VIRTIO_MMIO_QUEUE_DESC_LOW     {0x080};  // physical address for descriptor table, write-only
constexpr uint32_t VIRTIO_MMIO_QUEUE_DESC_HIGH    {0x084};
constexpr uint32_t VIRTIO_MMIO_DRIVER_DESC_LOW    {0x090};  // physical address for available ring, write-only
constexpr uint32_t VIRTIO_MMIO_DRIVER_DESC_HIGH   {0x094};
constexpr uint32_t VIRTIO_MMIO_DEVICE_DESC_LOW    {0x0a0};  // physical address for used ring, write-only
constexpr uint32_t VIRTIO_MMIO_DEVICE_DESC_HIGH   {0x0a4};

// status register bits, from qemu virtio_config.h
constexpr uint32_t VIRTIO_CONFIG_S_ACKNOWLEDGE  {1};
constexpr uint32_t VIRTIO_CONFIG_S_DRIVER       {2};
constexpr uint32_t VIRTIO_CONFIG_S_DRIVER_OK    {4};
constexpr uint32_t VIRTIO_CONFIG_S_FEATURES_OK  {8};

// device feature bits
constexpr uint32_t VIRTIO_BLK_F_RO              {5};  /* Disk is read-only */
constexpr uint32_t VIRTIO_BLK_F_SCSI            {7};  /* Supports scsi command passthru */
constexpr uint32_t VIRTIO_BLK_F_CONFIG_WCE      {11}; /* Writeback mode available in config */
constexpr uint32_t VIRTIO_BLK_F_MQ              {12}; /* support more than one vq */
constexpr uint32_t VIRTIO_F_ANY_LAYOUT          {27};
constexpr uint32_t VIRTIO_RING_F_INDIRECT_DESC  {28};
constexpr uint32_t VIRTIO_RING_F_EVENT_IDX      {29};

// this many virtio descriptors.
// must be a power of two.
constexpr uint32_t NUM {8};

// a single descriptor, from the spec.
struct virtq_desc {
  uint64_t addr;
  uint32_t len;
  uint16_t flags;
  uint16_t next;
};
constexpr uint32_t VRING_DESC_F_NEXT  {1};   // chained with another descriptor
constexpr uint32_t VRING_DESC_F_WRITE {2};  // device writes (vs read)

// the (entire) avail ring, from the spec.
struct virtq_avail {
  uint16_t flags;      // always zero
  uint16_t idx;        // driver will write ring[idx] next
  uint16_t ring[NUM];  // descriptor numbers of chain heads
  uint16_t unused;
};

// one entry in the "used" ring, with which the
// device tells the driver about completed requests.
struct virtq_used_elem {
  uint32_t id;  // index of start of completed descriptor chain
  uint32_t len;
};

struct virtq_used {
  uint16_t flags;  // always zero
  uint16_t idx;    // device increments when it adds a ring[] entry
  struct virtq_used_elem ring[NUM];
};

// these are specific to virtio block devices, e.g. disks,
// described in Section 5.2 of the spec.

constexpr uint32_t VIRTIO_BLK_T_IN  {0};   // read the disk
constexpr uint32_t VIRTIO_BLK_T_OUT {1};  // write the disk

// the format of the first descriptor in a disk request.
// to be followed by two more descriptors containing
// the block, and a one-byte status.
struct virtio_blk_req {
  uint32_t type;  // VIRTIO_BLK_T_IN or ..._OUT
  uint32_t reserved;
  uint64_t sector;
};

auto init() -> void;
auto disk_rw(struct bio::buf *b, bool write) -> void;
auto virtio_disk_intr() -> void;
}  // namespace virtio_disk
