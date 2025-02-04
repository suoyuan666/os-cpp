#include "vm"

#include <cstdint>
#include <cstring>

#include "arch/riscv"
#include "proc"

namespace vm {

extern "C" char end[];
extern "C" char etext[];
extern "C" char trampoline[];

kernel_vm::kernel_vm() {
  for (auto p_start = (uint64_t)end; p_start != PHY_END; p_start += PGSIZE) {
    free(reinterpret_cast<void *>(p_start));
  }
}

auto kernel_vm::free(void *addr) -> void {
  auto *tmp = static_cast<struct list *>(addr);
  tmp->next = freelist;
  freelist = tmp;
}
auto kernel_vm::alloc() -> std::optional<uint64_t *> {
  auto *tmp = freelist;
  if (tmp) {
    freelist = tmp->next;
    auto rs_val = reinterpret_cast<uint64_t *>(tmp);
    return {rs_val};
  }
  return {};
}

uint64_t *kernel_pagetable;
// kernel_vm kevm{};

auto kvm_make() -> std::optional<uint64_t *>;

auto init() -> void {
  auto opt_kpt = kvm_make();
  if (opt_kpt.has_value()) {
    kernel_pagetable = opt_kpt.value();
  }
}

auto kvm_make() -> std::optional<uint64_t *> {
  auto opt_kpt = kevm.alloc();
  if (opt_kpt.has_value()) {
    auto *kpt = opt_kpt.value();
    std::memset(kpt, 0, PGSIZE);
    map_pages(kpt, UART0, UART0, PGSIZE, PTE_R | PTE_W);
    map_pages(kpt, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);
    map_pages(kpt, PLIC, PLIC, 0x4000000, PTE_R | PTE_W);
    map_pages(kpt, KERNEL_BASE, KERNEL_BASE, (uint64_t)etext - KERNEL_BASE,
              PTE_R | PTE_X);
    map_pages(kpt, (uint64_t)etext, (uint64_t)etext, PHY_END - (uint64_t)etext,
              PTE_R | PTE_W);
    map_pages(kpt, TRAMPOLINE, (uint64_t)trampoline, PGSIZE, PTE_R | PTE_X);
    proc::map_stack(kpt);
    return {kpt};
  }
  return {};
}

auto walk(uint64_t *pagetable, uint64_t va, bool alloc)
    -> std::optional<uint64_t *> {
  for (int level{2}; level > 0; --level) {
    auto *pte = &pagetable[PX(level, va)];
    if (*pte & PTE_V) {
      pagetable = reinterpret_cast<uint64_t *>(PTE2PA(*pte));
    } else {
      if (alloc) {
        auto opt_pagetable = kevm.alloc();
        if (!opt_pagetable.has_value()) {
          return {};
        }
        pagetable = opt_pagetable.value();
      }
      std::memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(reinterpret_cast<uint64_t>(pagetable)) | PTE_V;
    }
  }

  return {&pagetable[PX(0, va)]};
}

auto map_pages(uint64_t *pagetable, uint64_t va, uint64_t pa, uint64_t size,
               uint32_t flag) -> bool {
  auto a = va;
  auto last = va + size - PGSIZE;

  while (a != last) {
    auto opt_pte = walk(pagetable, a, true);
    if (!opt_pte.has_value()) {
      return false;
    }
    auto pte = opt_pte.value();
    *pte = PA2PTE(pa) | flag | PTE_V;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return true;
}

auto inithart() -> void {
  sfence_vma();
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}

}  // namespace vm
