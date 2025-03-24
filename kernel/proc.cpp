#include "proc.h"

#include <cstdint>
#include <cstring>
#include <fmt>
#include <optional>

#ifndef ARCH_RISCV
#include "arch/riscv.h"
#define ARCH_RISCV
#endif

#include "file.h"
#include "fs.h"
#include "lock.h"
#include "log.h"
#include "trap.h"
#include "vm.h"

extern "C" char trampoline[];
extern "C" auto swtch(struct proc::context *, struct proc::context *) -> void;

namespace proc {

/* $ riscv64-linux-gnu-readelf -x .text build/utils/initcode
 *
 * Hex dump of section '.text':
 *   0x00000000 17050000 13054502 97050000 93858502 ......E.........
 *   0x00000010 93086000 73000000 93081000 73000000 ..`.s.......s...
 *   0x00000020 eff09fff 2f62696e 2f696e69 74000000 ..../bin/init...
 *   0x00000030 24000000 00000000 00000000 00000000 $...............
 */

// clang-format off
const unsigned char initcode[] = {
    0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
    0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x85, 0x02,
    0x93, 0x08, 0x60, 0x00, 0x73, 0x00, 0x00, 0x00,
    0x93, 0x08, 0x10, 0x00, 0x73, 0x00, 0x00, 0x00,
    0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x62, 0x69, 0x6e,
    0x2f, 0x69, 0x6e, 0x69, 0x74, 0x00, 0x00, 0x00,
    0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
// clang-format on

struct process proc_list[NPROC];
struct process *init_proc{};
struct cpu cpu_list[NCPU];
uint32_t next_pid{1};

class lock::spinlock wait_lock{};
class lock::spinlock pid_lock{};

auto forkret() -> void;

auto cpuid() -> uint32_t {
  auto id = r_tp();
  return id;
}
auto curr_cpu() -> struct cpu * {
  auto id = cpuid();
  return &cpu_list[id];
}
auto curr_proc() -> struct process * { return curr_cpu()->proc; }

auto init() -> void {
  for (auto &proc : proc_list) {
    proc.status = proc_status::UNUSED;
    proc.kernel_stack =
        vm::KSTACK((int)((uint64_t)&proc - (uint64_t)&proc_list[0]));
  }
}

auto map_stack(uint64_t *kpt) -> void {
  for (auto &proc : proc_list) {
    auto opt_pa = vm::kalloc();
    if (opt_pa.has_value()) {
      auto *pa = opt_pa.value();
      auto va = vm::KSTACK((int)((uint64_t)&proc - (uint64_t)proc_list));
      vm::map_pages(kpt, va, (uint64_t)pa, PGSIZE, PTE_R | PTE_W);
    } else {
      fmt::panic("proc::map_stack: error");
    }
  }
}

auto alloc_pid() -> uint32_t {
  pid_lock.acquire();
  auto pid = next_pid;
  next_pid++;
  pid_lock.release();
  return pid;
}

auto alloc_pagetable(struct process &p) -> uint64_t * {
  auto *uvm = vm::uvm_create();
  if (uvm == nullptr) {
    return nullptr;
  }

  if (vm::map_pages(uvm, vm::TRAMPOLINE, (uint64_t)trampoline, PGSIZE,
                    PTE_R | PTE_X) == false) {
    vm::uvm_free(uvm, 0);
    return nullptr;
  }
  if (vm::map_pages(uvm, vm::TRAPFRAME, (uint64_t)p.trapframe, PGSIZE,
                    PTE_R | PTE_W) == false) {
    vm::uvm_unmap(uvm, vm::TRAMPOLINE, 1, false);
    vm::uvm_free(uvm, 0);
    return nullptr;
  }
  return uvm;
}

auto free_pagetable(uint64_t *pagetable, uint64_t sz) -> void {
  vm::uvm_unmap(pagetable, vm::TRAMPOLINE, 1, false);
  vm::uvm_unmap(pagetable, vm::TRAPFRAME, 1, false);
  vm::uvm_free(pagetable, sz);
}

auto free(struct process *p) -> void {
  if (p->trapframe) {
    vm::kfree(p->trapframe);
  }
  p->trapframe = nullptr;
  if (p->pagetable) {
    free_pagetable(p->pagetable, p->sz);
  }
  p->pagetable = nullptr;
  p->sz = 0;
  p->pid = 0;
  p->parent = nullptr;
  p->name[0] = '\0';
  p->chan = nullptr;
  p->killed = false;
  p->status = proc_status::UNUSED;
}

auto alloc_proc() -> struct process * {
  for (auto &p : proc_list) {
    p.lock.acquire();
    if (p.status == proc_status::UNUSED) {
      p.pid = alloc_pid();
      p.status = proc_status::USED;

      auto opt_frame = vm::kalloc();
      if (!opt_frame.has_value()) {
        free(&p);
        p.lock.release();
        return nullptr;
      }
      p.trapframe = (struct trapframe *)opt_frame.value();

      p.pagetable = alloc_pagetable(p);
      if (p.pagetable == nullptr) {
        free(&p);
        p.lock.release();
        return nullptr;
      }

      std::memset(&p.context, 0, sizeof(p.context));
      p.context.ra = (uint64_t)forkret;
      p.context.sp = p.kernel_stack + PGSIZE;

      return &p;
    }
    p.lock.release();
  }
  fmt::panic("proc::alloc: no proc");

  return nullptr;
}

auto scheduler() -> void {
  auto *c = curr_cpu();
  c->proc = nullptr;

  while (true) {
    intr_on();

    auto found{false};
    for (uint32_t i{0}; i < NPROC; ++i) {
      auto *p = &proc_list[i];
      p->lock.acquire();
      if (p->status == proc_status::RUNNABLE) {
        p->status = proc_status::RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        c->proc = nullptr;
        found = true;
      }
      p->lock.release();
    }

    if (found == false) {
      intr_on();
      asm volatile("wfi");
    }
  }
}

auto sched() -> void {
  auto *p = curr_proc();

  if (!p->lock.holding()) {
    fmt::panic("proc::sched: should hold lock");
  }
  if (curr_cpu()->noff != 1) {
    fmt::panic("proc::sched: sched locks");
  }
  if (intr_get()) {
    fmt::panic("proc::sched: interruptible");
  }

  auto intena = curr_cpu()->intena;
  swtch(&p->context, &curr_cpu()->context);
  curr_cpu()->intena = intena;
}

auto yield() -> void {
  auto *p = curr_proc();
  p->lock.acquire();
  p->status = proc_status::RUNNABLE;
  sched();
  p->lock.release();
}

auto forkret() -> void {
  static bool first{true};

  curr_proc()->lock.release();

  if (first) {
    fs::init(1);
    fmt::print_log(fmt::log_level::INFO,
                   "the root file system init successful\n");
    first = false;
    __sync_synchronize();
  }

  trap::user_ret();
}

auto user_init() -> void {
  auto *p = alloc_proc();
  init_proc = p;

  vm::uvm_first(p->pagetable, (unsigned char *)initcode, sizeof(initcode));
  p->sz = PGSIZE;

  p->trapframe->epc = 0;
  p->trapframe->sp = PGSIZE;

  std::strncpy(p->name, "initcode", sizeof(p->name));
  p->cwd = fs::namei((char *)"/");
  p->status = proc_status::RUNNABLE;

  p->lock.release();
}

auto sleep(void *chan, class lock::spinlock &lock) -> void {
  auto *p = curr_proc();

  p->lock.acquire();
  lock.release();

  p->chan = chan;
  p->status = proc_status::SLEEPING;

  sched();

  p->chan = nullptr;
  p->lock.release();
  lock.acquire();
}

auto wakeup(void *chan) -> void {
  for (auto &p : proc_list) {
    if (&p != curr_proc()) {
      p.lock.acquire();
      if (p.status == proc_status::SLEEPING && p.chan == chan) {
        p.status = proc_status::RUNNABLE;
      }
      p.lock.release();
    }
  }
}

auto reparent(struct process &p) -> void {
  for (auto &cp : proc_list) {
    if (cp.parent == &p) {
      cp.parent = init_proc;
      wakeup(init_proc);
    }
  }
}

auto kill(uint32_t pid) -> int {
  for (auto &p : proc_list) {
    p.lock.acquire();
    if (p.pid == pid) {
      p.killed = true;
      if (p.status == proc_status::SLEEPING) {
        p.status = proc_status::RUNNABLE;
      }
      p.lock.release();
      return 0;
    }
    p.lock.release();
  }
  return -1;
}

auto set_killed(struct process *proc) -> void {
  proc->lock.acquire();
  proc->killed = true;
  proc->lock.release();
}

auto get_killed(struct process *proc) -> bool {
  proc->lock.acquire();
  auto rs = proc->killed;
  proc->lock.release();
  return rs;
}

auto grow(int n) -> int {
  auto *p = curr_proc();
  auto sz = p->sz;

  if (n > 0) {
    if ((sz = vm::uvm_alloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if (n < 0) {
    sz = vm::uvm_dealloc(p->pagetable, sz, sz + n);
  }

  p->sz = sz;
  return 0;
}

auto fork() -> int32_t {
  auto *np = alloc_proc();
  if (np == nullptr) {
    return -1;
  }

  auto *p = curr_proc();

  if (vm::uvm_copy(p->pagetable, np->pagetable, p->sz) == false) {
    free(np);
    np->lock.release();
    return -1;
  }
  np->sz = p->sz;

  *(np->trapframe) = *(p->trapframe);

  np->trapframe->a0 = 0;

  for (uint32_t i{0}; i < file::NOFILE; ++i) {
    if (p->ofile[i] != nullptr) {
      np->ofile[i] = file::dup(p->ofile[i]);
    }
  }
  np->cwd = fs::idup(p->cwd);

  std::strncpy(np->name, p->name, sizeof(p->name));

  auto rs = np->pid;

  np->lock.release();

  wait_lock.acquire();
  np->parent = p;
  wait_lock.release();

  np->lock.acquire();
  np->status = proc_status::RUNNABLE;
  np->lock.release();

  return static_cast<int32_t>(rs);
}

auto exit(int status) -> void {
  auto *p = curr_proc();
  if (p == init_proc) {
    fmt::panic("proc::exit: init exiting");
  }

  for (uint32_t fd{0}; fd < file::NOFILE; ++fd) {
    if (p->ofile[fd] != nullptr) {
      file::close(p->ofile[fd]);
      p->ofile[fd] = nullptr;
    }
  }

  log::begin_op();
  fs::iput(p->cwd);
  log::end_op();
  p->cwd = nullptr;

  wait_lock.acquire();
  reparent(*p);
  wakeup(p->parent);

  p->lock.acquire();

  p->xstate = status;
  p->status = proc_status::ZOMBIE;

  wait_lock.release();

  sched();
  fmt::panic("proc::exit: zombie exit");
}

auto wait(uint64_t addr) -> int {
  wait_lock.acquire();
  auto *p = curr_proc();

  while (true) {
    auto havekids{false};

    for (auto &cp : proc_list) {
      if (cp.parent == p) {
        cp.lock.acquire();

        havekids = true;
        if (cp.status == proc_status::ZOMBIE) {
          auto pid = cp.pid;
          if (addr != 0 && vm::copyout(p->pagetable, addr, (char *)&cp.xstate,
                                       sizeof(cp.xstate)) == false) {
            cp.lock.release();
            wait_lock.release();
            return -1;
          }
          free(&cp);
          cp.lock.release();
          wait_lock.release();
          return static_cast<int32_t>(pid);
        }

        cp.lock.release();
      }
    }

    if (havekids == false || get_killed(p)) {
      wait_lock.release();
      return -1;
    }

    sleep(p, wait_lock);
  }
}

auto dump(struct process &p) -> void {
  fmt::print("pid: {}, name: {} ", p.pid, p.name);
  switch (p.status) {
    case proc_status::UNUSED:
      fmt::print("status: UNUSED");
      break;
    case proc_status::USED:
      fmt::print("status: USED");
      break;
    case proc_status::SLEEPING:
      fmt::print("status: SLEEPING");
      break;
    case proc_status::RUNNABLE:
      fmt::print("status: RUNNABLE");
      break;
    case proc_status::RUNNING:
      fmt::print("status: RUNNING");
      break;
    case proc_status::ZOMBIE:
      fmt::print("status: ZOMBIE");
      break;
  }
  fmt::print("\n");
}

auto either_copyout(bool user_dst, uint64_t dst, void *src, uint64_t len)
    -> int {
  auto *p = curr_proc();
  if (user_dst) {
    return vm::copyout(p->pagetable, dst, (char *)src, len);
  }
  std::memmove((char *)dst, src, len);
  return 0;
}

auto either_copyin(void *dst, bool user_dst, uint64_t src, uint64_t len)
    -> int {
  auto *p = curr_proc();
  if (user_dst) {
    return vm::copyin(p->pagetable, (char *)dst, src, len);
  }
  std::memmove(dst, (char *)src, len);
  return 0;
}
}  // namespace proc
