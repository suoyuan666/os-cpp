#include "vm"

#include <cstdint>
#include <cstring>
#include <optional>

#include "arch/riscv"
#include "fmt"
#include "proc"

namespace vm {

extern "C" char etext[];

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

  uint64_t *rs = &pagetable[PX(0, va)];
  return {rs};
}

auto walkaddr(uint64_t *pagetable, uint64_t va) -> uint64_t {
  if (va > VA_MAX) {
    return 0;
  }

  auto opt_pte = walk(pagetable, va, false);
  if (!opt_pte.has_value()) {
    return 0;
  }
  auto *pte = opt_pte.value();
  if ((*pte & PTE_V) == 0) {
    return 0;
  }
  if ((*pte & PTE_U) == 0) {
    return 0;
  }

  return PTE2PA(*pte);
}

auto free_walk(uint64_t *pagetable) -> void {
  for (auto i{0}; i < 512; ++i) {
    auto pte = pagetable[i];
    if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0) {
      auto child = PTE2PA(pte);
      free_walk((uint64_t *)child);
      pagetable[i] = 0;
    } else if (pte & PTE_V) {
      fmt::panic("vm::free_walk: leaf");
    }
  }
  kevm.free(pagetable);
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

auto uvm_alloc() -> uint64_t * {
  uint64_t *rs = kevm.alloc().value();
  std::memset(rs, 0, PGSIZE);
  return rs;
}

auto uvm_first(uint64_t *pagetable, unsigned char *src, uint32_t size) -> void {
  if (size > PGSIZE) {
    fmt::panic("uvm_first: size > PGSIZE");
  }

  auto *mem = (char *)kevm.alloc().value();
  std::memset(mem, 0, PGSIZE);
  map_pages(pagetable, 0, (uint64_t)mem, PGSIZE, PTE_R | PTE_W | PTE_X | PTE_U);
  std::memmove(mem, src, size);
}

auto uvm_unmap(uint64_t *pagetable, uint64_t va, uint64_t npages, bool do_free)
    -> void {
  if ((va % PGSIZE) != 0) {
    fmt::panic("vm::uvm_unmap: not aligned");
  }

  for (auto a = va; a < va + npages * PGSIZE; a += PGSIZE) {
    auto opt_pte = walk(pagetable, a, false);
    if (!opt_pte.has_value()) {
      fmt::panic("vm::uvm_unmap: walk");
    }
    auto *pte = opt_pte.value();
    if ((*pte & PTE_V) == 0) {
      fmt::panic("vm::uvm_unmap: not mapped");
    }
    if (PTE_FLAGS(*pte) == PTE_V) {
      fmt::panic("vm::uvm_unmap: not a leaf");
    }
    if (do_free) {
      auto pa = PTE2PA(*pte);
      kevm.free((void *)pa);
    }
    *pte = 0;
  }
}

auto uvm_free(uint64_t *pagetable, uint64_t sz) -> void {
  if (sz > 0) {
    uvm_unmap(pagetable, 0, PG_ROUND_UP(sz) / PGSIZE, true);
  }
  free_walk(pagetable);
}

auto uvm_alloc(uint64_t *pagetable, uint64_t oldsz, uint64_t newsz, int xperm)
    -> uint64_t {
  if (oldsz > newsz) {
    return oldsz;
  }

  oldsz = PG_ROUND_UP(oldsz);
  for (auto a{oldsz}; a < newsz; a += PGSIZE) {
    auto opt_mem = kevm.alloc();
    if (!opt_mem.has_value()) {
      uvm_dealloc(pagetable, a, oldsz);
      return 0;
    }
    auto *mem = opt_mem.value();
    std::memset(mem, 0, PGSIZE);
    if (map_pages(pagetable, a, (uint64_t)mem, PGSIZE, PTE_R | PTE_U | xperm) ==
        false) {
      kevm.free(mem);
      uvm_dealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

auto uvm_dealloc(uint64_t *pagetable, uint64_t oldsz, uint64_t newsz)
    -> uint64_t {
  if (newsz >= oldsz) {
    return oldsz;
  }

  if (PG_ROUND_UP(newsz) < PG_ROUND_UP(oldsz)) {
    auto npages = (PG_ROUND_UP(oldsz) - PG_ROUND_UP(newsz)) / PGSIZE;
    uvm_unmap(pagetable, PG_ROUND_UP(newsz), npages, true);
  }

  return newsz;
}

auto uvm_copy(uint64_t *old_pagetable, uint64_t *new_pagetable, uint64_t sz)
    -> bool {
  for (uint32_t i{0}; i < sz; ++i) {
    auto opt_pte = walk(old_pagetable, i, false);
    if (!opt_pte.has_value()) {
      fmt::panic("vm::uvm_copy: pte not exit");
    }
    auto *pte = opt_pte.value();
    if ((*pte & PTE_V) == 0) {
      fmt::panic("vm::uvm_copy: page not present");
    }

    auto pa = PTE2PA(*pte);
    auto flags = PTE_FLAGS(*pte);

    auto opt_mem = kevm.alloc();
    if (!opt_mem.has_value()) {
      uvm_unmap(new_pagetable, i, i / PGSIZE, true);
      return false;
    }
    auto *mem = opt_mem.value();
    std::memmove(mem, (char *)pa, PGSIZE);

    if (map_pages(new_pagetable, i, (uint64_t)mem, PGSIZE, flags) == false) {
      uvm_unmap(new_pagetable, i, i / PGSIZE, true);
      return false;
    }
  }
  return true;
}

auto uvm_clear(uint64_t *pagetable, uint64_t va) -> void {
  auto opt_pte = walk(pagetable, va, false);
  if (!opt_pte.has_value()) {
    fmt::panic("vm::uvm_clear: pte not exit");
  }
  auto *pte = opt_pte.value();
  *pte &= ~PTE_V;
}

auto copyout(uint64_t *pagetable, uint64_t dstva, char *src, uint64_t len)
    -> bool {
  while (len > 0) {
    auto va0 = PG_ROUND_DOWN(dstva);
    if (va0 > VA_MAX) {
      return false;
    }

    auto opt_pte = walk(pagetable, va0, false);
    if (!opt_pte.has_value()) {
      return false;
    }
    auto *pte = opt_pte.value();
    if ((*pte & PTE_V) == 0 || (*pte & PTE_U) == 0 || (*pte & PTE_W) == 0) {
      return false;
    }

    auto pa0 = PTE2PA(*pte);
    auto n = PGSIZE - (dstva - va0);

    if (n > len) {
      n = len;
    }

    std::memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }

  return true;
}

auto copyin(uint64_t *pagetable, char *dst, uint64_t srcva, uint64_t len)
    -> bool {
  while (len > 0) {
    auto va0 = PG_ROUND_DOWN(srcva);
    auto pa0 = walkaddr(pagetable, va0);

    if (pa0 == 0) {
      return false;
    }

    auto n = PGSIZE - (srcva - va0);
    if (n > len) {
      n = len;
    }

    std::memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }

  return true;
}
}  // namespace vm
