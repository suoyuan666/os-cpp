#include "loader.h"

#include <cstdint>
#include <cstring>

#include "arch/riscv.h"
#include "elf.h"
#include "file.h"
#include "fs.h"
#include "log.h"
#include "proc.h"
#include "vm.h"

namespace loader {
auto loadseg(uint64_t *pagetable, uint64_t va, struct file::inode *ip,
             uint32_t offset, uint32_t sz) -> bool {
  uint32_t i = 0, n = 0;
  uint64_t pa = 0;

  for (i = 0; i < sz; i += PGSIZE) {
    pa = vm::walkaddr(pagetable, va + i);
    if (pa == 0) {
      fmt::panic("loadseg: address should exist");
    }
    if (sz - i < PGSIZE) {
      n = sz - i;
    } else {
      n = PGSIZE;
    }
    if (fs::readi(ip, false, (uint64_t)pa, offset + i, n) != n) {
      return false;
    }
  }

  return true;
}

auto flags2perm(uint32_t flags) -> uint32_t {
  uint32_t perm = 0;
  if (flags & 0x1U) {
    perm = PTE_X;
  }
  if (flags & 0x2U) {
    perm |= PTE_W;
  }
  return perm;
}

auto fail_work(uint64_t *pagetable, struct file::inode *&ip, uint64_t sz)
    -> void {
  if (pagetable) {
    proc::free_pagetable(pagetable, sz);
  }
  if (ip) {
    fs::iunlockput(ip);
    log::end_op();
  }
}

auto exec(char *path, char **argv) -> int {
  char *s = nullptr, *last = nullptr;
  int i = 0;
  uint64_t off = 0;
  uint64_t argc = 0, sz = 0, sp = 0, ustack[MAXARGV], stackbase = 0;
  struct elfhdr elf{};
  struct file::inode *ip = nullptr;
  struct proghdr ph{};
  uint64_t *pagetable = nullptr;
  uint64_t *oldpagetable = nullptr;
  auto *p = proc::curr_proc();

  log::begin_op();

  if ((ip = fs::namei(path)) == nullptr) {
    log::end_op();
    return -1;
  }
  fs::ilock(ip);

  if (fs::readi(ip, false, (uint64_t)&elf, 0, sizeof(elf)) != sizeof(elf)) {
    fail_work(pagetable, ip, sz);
    return -1;
  }

  if (elf.magic != ELF_MAGIC) {
    fail_work(pagetable, ip, sz);
    return -1;
  }

  pagetable = proc::alloc_pagetable(*p);
  if (pagetable == nullptr) {
    fail_work(pagetable, ip, sz);
    return -1;
  }

  for (i = 0, off = elf.phoff; i < elf.phnum; i++, off += sizeof(ph)) {
    if (fs::readi(ip, false, (uint64_t)&ph, off, sizeof(ph)) != sizeof(ph)) {
      fail_work(pagetable, ip, sz);
      return -1;
    }
    if (ph.type != ELF_PROG_LOAD) {
      continue;
    }
    if (ph.memsz < ph.filesz) {
      fail_work(pagetable, ip, sz);
      return -1;
    }
    if (ph.vaddr + ph.memsz < ph.vaddr) {
      fail_work(pagetable, ip, sz);
      return -1;
    }
    if (ph.vaddr % PGSIZE != 0) {
      fail_work(pagetable, ip, sz);
      return -1;
    }
    auto sz1 =
        vm::uvm_alloc(pagetable, sz, ph.vaddr + ph.memsz, flags2perm(ph.flags));
    if (sz1 == 0) {
      fail_work(pagetable, ip, sz);
      return -1;
    }
    sz = sz1;
    if (!loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz)) {
      fail_work(pagetable, ip, sz);
      return -1;
    }
  }
  fs::iunlockput(ip);
  log::end_op();
  ip = nullptr;

  p = proc::curr_proc();
  uint64_t oldsz = p->sz;

  sz = PG_ROUND_UP(sz);
  uint64_t sz1 = vm::uvm_alloc(
      pagetable, sz, sz + ((static_cast<uint64_t>(USERSTACK + 1)) * PGSIZE),
      PTE_W);
  if (sz1 == 0) {
    fail_work(pagetable, ip, sz);
    return -1;
  }
  sz = sz1;
  vm::uvm_clear(pagetable,
                sz - static_cast<uint64_t>((USERSTACK + 1) * PGSIZE));
  sp = sz;
  stackbase = sp - static_cast<uint64_t>(USERSTACK * PGSIZE);

  // Push argument strings, prepare rest of stack in ustack.
  for (argc = 0; argv[argc]; argc++) {
    if (argc >= MAXARGV) {
      fail_work(pagetable, ip, sz);
      return -1;
    }
    sp -= std::strlen(argv[argc]) + 1;
    sp -= sp % 16;  // riscv sp must be 16-byte aligned
    if (sp < stackbase) {
      fail_work(pagetable, ip, sz);
      return -1;
    }
    if (vm::copyout(pagetable, sp, argv[argc], std::strlen(argv[argc]) + 1) ==
        false) {
      fail_work(pagetable, ip, sz);
      return -1;
    }
    ustack[argc] = sp;
  }
  ustack[argc] = 0;

  // push the array of argv[] pointers.
  sp -= (argc + 1) * sizeof(uint64_t);
  sp -= sp % 16;
  if (sp < stackbase) {
    fail_work(pagetable, ip, sz);
    return -1;
  }
  if (vm::copyout(pagetable, sp, (char *)ustack,
                  (argc + 1) * sizeof(uint64_t)) == false) {
    fail_work(pagetable, ip, sz);
    return -1;
  }

  // arguments to user main(argc, argv)
  // argc is returned via the system call return
  // value, which goes in a0.
  p->trapframe->a1 = sp;

  // Save program name for debugging.
  for (last = s = path; *s; s++) {
    if (*s == '/') {
      last = s + 1;
    }
  }
  std::strncpy(p->name, last, sizeof(p->name));

  // Commit to the user image.
  oldpagetable = p->pagetable;
  p->pagetable = pagetable;
  p->sz = sz;
  p->trapframe->epc = elf.entry;
  p->trapframe->sp = sp;
  proc::free_pagetable(oldpagetable, oldsz);

  // clang-format off
  return static_cast<int>(argc);  // this ends up in a0, the first argument to main(argc, argv)
}
}  // namespace loader
