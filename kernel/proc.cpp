#include "proc"

#include <array>
#include <cstdint>

#include "arch/riscv"
#include "vm"

namespace proc {
process proc_list[NPROC];
uint32_t next_pid{1};

auto init() -> void {
  for (auto &proc : proc_list) {
    proc.status = proc_status::UNUSED;
    proc.kernel_stack = vm::KSTACK((uint64_t)&proc - (uint64_t)&proc_list);
  }
}

auto map_stack(uint64_t *kpt) -> void {
  for (auto &proc : proc_list) {
    auto opt_pa = vm::kevm.alloc();
    if (opt_pa.has_value()) {
      auto *pa = opt_pa.value();
      auto va = vm::KSTACK((uint64_t)pa - (uint64_t)&proc);
      vm::map_pages(kpt, va, (uint64_t)pa, PGSIZE, PTE_R | PTE_W);
    }
  }
}

}  // namespace proc
