#include "vm.h"

#include <cstdint>
#include <cstring>
#include <fmt>
#include <optional>

#ifndef ARCH_RISCV
#include "arch/riscv.h"
#define ARCH_RISCV
#endif

#include "proc.h"

extern "C" char end[];
extern "C" char trampoline[];
extern "C" char etext[];

namespace vm {
uint64_t *kernel_pagetable;
struct {
  class lock::spinlock lock{};
  struct list *freelist{};
} kmem{};

auto kvm_make() -> std::optional<uint64_t *>;

auto kinit() -> void {
  for (auto *p_start = (char *)PG_ROUND_UP((uint64_t)end);
       p_start + PGSIZE <= (char *)PHY_END; p_start += PGSIZE) {
    kfree(p_start);
  }
}

auto kfree(void *addr) -> void {
  std::memset(addr, 1, PGSIZE);
  auto *tmp = static_cast<struct list *>(addr);
  kmem.lock.acquire();
  tmp->next = kmem.freelist;
  kmem.freelist = tmp;
  kmem.lock.release();
}

auto kalloc() -> std::optional<uint64_t *> {
  kmem.lock.acquire();
  auto *tmp = kmem.freelist;
  if (tmp) {
    kmem.freelist = tmp->next;
    std::memset((char *)tmp, 5, PGSIZE);  // fill with junk
    auto rs_val = (uint64_t *)(tmp);
    kmem.lock.release();
    return {rs_val};
  }
  kmem.lock.release();
  return {};
}

auto init() -> void {
  auto opt_kpt = kvm_make();
  if (opt_kpt.has_value()) {
    kernel_pagetable = opt_kpt.value();
  } else {
    fmt::panic("vm::init: error");
  }
}

auto kvm_make() -> std::optional<uint64_t *> {
  auto opt_kpt = kalloc();
  if (opt_kpt.has_value()) {
    auto *kpt = opt_kpt.value();
    std::memset(kpt, 0, PGSIZE);
    if (map_pages(kpt, UART0, UART0, PGSIZE, PTE_R | PTE_W) == false) {
      fmt::panic("vm::kvm_make: UART0");
    }
    if (map_pages(kpt, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W) == false) {
      fmt::panic("vm::kvm_make: VIRTIO0");
    }
    if (map_pages(kpt, PLIC, PLIC, 0x4000000, PTE_R | PTE_W) == false) {
      fmt::panic("vm::kvm_make: PLIC");
    }
    if (map_pages(kpt, KERNEL_BASE, KERNEL_BASE, (uint64_t)etext - KERNEL_BASE,
                  PTE_R | PTE_X) == false) {
      fmt::panic("vm::kvm_make: KERNEL_BASE");
    }
    if (map_pages(kpt, (uint64_t)etext, (uint64_t)etext,
                  PHY_END - (uint64_t)etext, PTE_R | PTE_W) == false) {
      fmt::panic("vm::kvm_make: etext");
    }
    if (map_pages(kpt, TRAMPOLINE, (uint64_t)trampoline, PGSIZE,
                  PTE_R | PTE_X) == false) {
      fmt::panic("vm::kvm_make: TRAMPOLINE");
    }
    proc::map_stack(kpt);
    return {kpt};
  }
  return {};
}

auto walk(uint64_t *pagetable, uint64_t va, bool alloc)
    -> std::optional<uint64_t *> {
  if (va > VA_MAX) {
    fmt::panic("vm::walk: va > VA_MAX");
  }

  for (int level{2}; level > 0; --level) {
    auto *pte = &pagetable[PX(level, va)];
    if (*pte & PTE_V) {
      pagetable = reinterpret_cast<uint64_t *>(PTE2PA(*pte));
    } else {
      if (alloc) {
        auto opt_pagetable = vm::kalloc();
        if (!opt_pagetable.has_value()) {
          return {};
        }
        pagetable = opt_pagetable.value();
      } else {
        return {};
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
  kfree(pagetable);
}

auto map_pages(uint64_t *pagetable, uint64_t va, uint64_t pa, uint64_t size,
               uint32_t flag) -> bool {
  if ((va % PGSIZE) != 0) {
    fmt::panic("mappages: va not aligned");
  }
  if ((size % PGSIZE) != 0) {
    fmt::panic("mappages: size not aligned");
  }
  if (size == 0) {
    fmt::panic("mappages: size");
  }

  auto a = va;
  auto last = va + size - PGSIZE;

  while (true) {
    auto opt_pte = walk(pagetable, a, true);
    if (!opt_pte.has_value()) {
      return false;
    }
    auto *pte = opt_pte.value();
    if (*pte & PTE_V) {
      fmt::panic("vm::mappages: remap");
    }
    *pte = PA2PTE(pa) | flag | PTE_V;
    if (a == last) {
      break;
    }
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

auto uvm_create() -> uint64_t * {
  auto opt_rs = kalloc();
  if (!opt_rs.has_value()) {
    return nullptr;
  }
  auto *rs = opt_rs.value();
  std::memset(rs, 0, PGSIZE);
  return rs;
}

auto uvm_first(uint64_t *pagetable, unsigned char *src, uint32_t size) -> void {
  if (size > PGSIZE) {
    fmt::panic("uvm_first: size > PGSIZE");
  }

  auto *mem = (char *)kalloc().value();
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
      continue;
      // fmt::panic("vm::uvm_unmap: not mapped");
    }
    if (PTE_FLAGS(*pte) == PTE_V) {
      fmt::panic("vm::uvm_unmap: not a leaf");
    }
    if (do_free) {
      auto pa = PTE2PA(*pte);
      kfree((void *)pa);
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

auto uvm_alloc(uint64_t *pagetable, uint64_t oldsz, const uint64_t newsz,
               uint32_t xperm) -> uint64_t {
  if (oldsz > newsz) {
    return oldsz;
  }

  oldsz = PG_ROUND_UP(oldsz);
  for (auto a{oldsz}; a < newsz; a += PGSIZE) {
    auto opt_mem = kalloc();
    if (!opt_mem.has_value()) {
      uvm_dealloc(pagetable, a, oldsz);
      return 0;
    }
    auto *mem = opt_mem.value();
    std::memset(mem, 0, PGSIZE);
    if (map_pages(pagetable, a, (uint64_t)mem, PGSIZE, PTE_R | PTE_U | xperm) ==
        false) {
      kfree(mem);
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
  for (uint64_t i{0}; i < sz; i += PGSIZE) {
    auto opt_pte = walk(old_pagetable, i, false);
    if (!opt_pte.has_value()) {
      fmt::panic("vm::uvm_copy: pte not exit");
    }
    auto *pte = opt_pte.value();
    if ((*pte & PTE_V) == 0) {
      continue;
      // fmt::panic("vm::uvm_copy: page not present");
    }

    auto pa = PTE2PA(*pte);
    auto flags = PTE_FLAGS(*pte);

    auto opt_mem = kalloc();
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
  uint64_t va0 = 0;
  uint64_t pa0 = 0;
  uint64_t n = 0;

  while (len > 0) {
    va0 = PG_ROUND_DOWN(srcva);
    pa0 = walkaddr(pagetable, va0);

    if (pa0 == 0) {
      return false;
    }

    n = PGSIZE - (srcva - va0);
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

auto copyinstr(uint64_t *pagetable, char *dst, uint64_t srcva, uint64_t len)
    -> bool {
  auto fetch_null{false};

  while (fetch_null == false && len > 0) {
    auto va0 = PG_ROUND_DOWN(srcva);
    auto pa0 = walkaddr(pagetable, va0);
    if (pa0 == 0) {
      return false;
    }

    auto n = PGSIZE - (srcva - va0);
    if (n > len) {
      n = len;
    }

    auto *p = (char *)(pa0 + (srcva - va0));
    while (n > 0) {
      if (*p == '\0') {
        *dst = '\0';
        fetch_null = true;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --len;
      ++p;
      ++dst;
    }

    srcva = va0 + PGSIZE;
  }

  return fetch_null;
}
}  // namespace vm
