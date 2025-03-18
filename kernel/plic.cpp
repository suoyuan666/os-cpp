#include <cstdint>

#include "proc.h"
#include "vm.h"

namespace plic {
auto init() -> void {
  // set desired IRQ priorities non-zero (otherwise disabled).
  *(uint32_t*)(vm::PLIC + vm::UART_IRQ * 4) = 1;
  *(uint32_t*)(vm::PLIC + vm::VIRTIO_IRQ * 4) = 1;
}

auto inithart() -> void {
  auto id = proc::cpuid();
  // set enable bits for this hart's S-mode
  // for the uart and virtio disk.
  *(uint32_t*)PLIC_SENABLE(id) =
      (1U << vm::UART_IRQ) | (1U << vm::VIRTIO_IRQ);

  // set this hart's S-mode priority threshold to 0.
  *(uint32_t*)PLIC_SPRIORITY(id) = 0;
}

auto plic_claim() -> int {
  auto id = proc::cpuid();
  int rs = *(uint32_t*)PLIC_SCLAIM(id);
  return rs;
}

auto plic_complete(int irq) -> void {
  auto id = proc::cpuid();
  *(uint32_t*)PLIC_SCLAIM(id) = irq;
}
}  // namespace plic
