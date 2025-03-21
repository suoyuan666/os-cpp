#pragma once
#include <cstdint>
#include <fmt>
#include <optional>

#include "lock.h"

#ifndef ARCH_RISCV
#include "arch/riscv.h"
#define ARCH_RISCV
#endif

namespace vm {
// qemu -machine virt is set up like this,
// based on qemu's hw/riscv/virt.c:
//
// 00001000 -- boot ROM, provided by qemu
// 02000000 -- CLINT
// 0C000000 -- PLIC
// 10000000 -- uart0
// 10001000 -- virtio disk
// 80000000 -- boot ROM jumps here in machine mode
//             -kernel loads the kernel here
// unused RAM after 80000000.

// the kernel uses physical memory thus:
// 80000000 -- entry.S, then kernel text and data
// end -- start of kernel page allocation area
// PHYSTOP -- end RAM used by the kernel

constexpr uint64_t UART0{0x10000000ULL};
constexpr uint64_t UART_IRQ{10ULL};

constexpr uint64_t VIRTIO0{0x10001000U};
constexpr uint64_t VIRTIO_IRQ{1U};

constexpr uint64_t PLIC{0x0c000000ULL};
constexpr uint64_t PLIC_PRIORITY{PLIC + 0x0};
constexpr uint64_t PLIC_PENDING{PLIC + 0x1000};

// #define PLIC_SENABLE(hart) (vm::PLIC + 0x2080 + (hart) * 0x100)

// auto set_plic_senable(uint32_t hart, uint32_t value) -> void {
//   auto *addr = reinterpret_cast<uint64_t *>(
//       static_cast<uint64_t>(hart * 0x100) + vm::PLIC + 0x2080);
//   *addr = value;
// }
// #define PLIC_SPRIORITY(hart) (vm::PLIC + 0x201000 + (hart) * 0x2000)

// auto set_plic_spriority(uint32_t hart, uint32_t value) -> void {
//   auto *addr = reinterpret_cast<uint64_t *>(
//       static_cast<uint64_t>(hart * 0x2000) + vm::PLIC + 0x201000);
//   *addr = value;
// }

// #define PLIC_SCLAIM(hart) (vm::PLIC + 0x201004 + (hart) * 0x2000)

// auto get_plic_sclaim(uint32_t id) -> uint64_t {
//   auto *addr = reinterpret_cast<uint64_t *>(static_cast<uint64_t>(id * 0x2000) +
//                                             vm::PLIC + 0x201004);
//   return *addr;
// }
//
// auto set_plic_sclaim(uint32_t hart, uint32_t value) -> void {
//   auto *addr = reinterpret_cast<uint64_t *>(
//       static_cast<uint64_t>(hart * 0x2000) + vm::PLIC + 0x201004);
//   *addr = value;
// }

//
// static inline auto PLIC_SENABLE(uint64_t hart) -> uint64_t {
//   return PLIC + 0x2080 + (hart) * 0x100;
// };
// static inline auto PLIC_SPRIORITY(uint64_t hart) -> uint64_t {
//   return PLIC + 0x201000 + (hart) * 0x2000;
// };
// static inline auto PLIC_SCLAIM(uint64_t hart) -> uint64_t {
//   return PLIC + 0x201004 + (hart) * 0x2000;
// };

constexpr uint64_t KERNEL_BASE{0x80000000ULL};
constexpr uint64_t PHY_END{KERNEL_BASE + 128ULL * 1024ULL * 1024ULL};

constexpr uint64_t TRAMPOLINE{VA_MAX - PGSIZE};
constexpr uint64_t TRAPFRAME{TRAMPOLINE - PGSIZE};
static inline auto KSTACK(int pa) -> uint64_t {
  return TRAMPOLINE - static_cast<uint64_t>((pa + 1) * 2) * PGSIZE;
};

struct list {
  struct list *next;
};

auto kinit() -> void;
auto kfree(void *addr) -> void;
auto kalloc() -> std::optional<uint64_t *>;
auto init() -> void;
auto map_pages(uint64_t *pagetable, uint64_t va, uint64_t pa, uint64_t size,
               uint32_t flag) -> bool;
auto inithart() -> void;
auto uvm_create() -> uint64_t *;
auto walkaddr(uint64_t *pagetable, uint64_t va) -> uint64_t;
auto uvm_first(uint64_t *pagetable, unsigned char *src, uint32_t size) -> void;
auto uvm_unmap(uint64_t *pagetable, uint64_t va, uint64_t npages, bool do_free)
    -> void;
auto uvm_free(uint64_t *pagetable, uint64_t sz) -> void;
auto uvm_clear(uint64_t *pagetable, uint64_t va) -> void;
auto uvm_dealloc(uint64_t *pagetable, uint64_t oldsz, uint64_t newsz)
    -> uint64_t;
auto uvm_alloc(uint64_t *pagetable, uint64_t oldsz, const uint64_t newsz,
               uint32_t xperm) -> uint64_t;
auto copyin(uint64_t *pagetable, char *dst, uint64_t srcva, uint64_t len)
    -> bool;
auto copyout(uint64_t *pagetable, uint64_t dstva, char *src, uint64_t len)
    -> bool;
auto uvm_copy(uint64_t *old_pagetable, uint64_t *new_pagetable, uint64_t sz)
    -> bool;
auto copyinstr(uint64_t *pagetable, char *dst, uint64_t srcva, uint64_t len)
    -> bool;
}  // namespace vm
