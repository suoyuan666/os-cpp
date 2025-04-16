#include <cstdint>

#include "proc.h"
#include "vm.h"

namespace plic {

auto set_plic_senable(uint32_t hart, uint32_t value) -> void {
  auto *addr = reinterpret_cast<uint32_t *>(
      static_cast<uint64_t>(hart * 0x100) + vm::PLIC + 0x2080);
  *addr = value;
}

auto set_plic_spriority(uint32_t hart, uint32_t value) -> void {
  auto *addr = reinterpret_cast<uint32_t *>(
      static_cast<uint64_t>(hart * 0x2000) + vm::PLIC + 0x201000);
  *addr = value;
}
auto get_plic_sclaim(uint32_t id) -> uint32_t {
  auto *addr = reinterpret_cast<uint32_t *>(static_cast<uint64_t>(id * 0x2000) +
                                            vm::PLIC + 0x201004);
  return *addr;
}

auto set_plic_sclaim(uint32_t hart, uint32_t value) -> void {
  auto *addr = reinterpret_cast<uint64_t *>(
      static_cast<uint64_t>(hart * 0x2000) + vm::PLIC + 0x201004);
  *addr = value;
}

auto init() -> void {
  *reinterpret_cast<uint32_t *>(vm::PLIC + vm::UART_IRQ * 4) = 1;
  *reinterpret_cast<uint32_t *>(vm::PLIC + vm::VIRTIO_IRQ * 4) = 1;
}

auto inithart() -> void {
  // clang-format off
  auto id = proc::cpuid();
  set_plic_senable(id, (1U << vm::UART_IRQ) | (1U << vm::VIRTIO_IRQ));
  set_plic_spriority(id, 0U);
  // clang-format on
}

auto plic_claim() -> int {
  auto id = proc::cpuid();
  auto rs = static_cast<int>(get_plic_sclaim(id));
  return rs;
}

auto plic_complete(int irq) -> void {
  auto id = proc::cpuid();
  set_plic_sclaim(id, irq);
}
}  // namespace plic
