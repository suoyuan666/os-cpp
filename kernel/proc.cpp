#include "proc"

#include <cstdint>
#include <cstring>
#include <optional>

#include "arch/riscv"
#include "file"
#include "fmt"
#include "fs"
#include "lock"
#include "trap"
#include "vm"

namespace proc {

constexpr unsigned char initcode[] = {
    0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02, 0x97, 0x05, 0x00,
    0x00, 0x93, 0x85, 0x35, 0x02, 0x93, 0x08, 0x70, 0x00, 0x73, 0x00,
    0x00, 0x00, 0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00, 0xef,
    0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69, 0x74, 0x00, 0x00, 0x24,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

process proc_list[NPROC];
process *init_proc;
struct cpu *cpu;
uint32_t next_pid{1};

lock::spinlock wait_lock;

auto forkret() -> void;
extern "C" auto swtch(struct context *, struct context *) -> void;

auto curr_cpu() -> struct cpu * { return cpu; }
auto curr_proc() -> struct process * { return curr_cpu()->proc; }

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

auto alloc_pid() -> uint32_t { return next_pid++; }

auto alloc_pagetable(struct process &p) -> uint64_t * {
  auto *uvm = vm::uvm_alloc();
  vm::map_pages(uvm, vm::TRAMPOLINE, (uint64_t)vm::trampoline, PGSIZE,
                PTE_R | PTE_X);
  vm::map_pages(uvm, vm::TRAMFRAME, (uint64_t)p.trapframe, PGSIZE,
                PTE_R | PTE_W);
  return uvm;
}

auto free_pagetable(uint64_t *pagetable, uint64_t sz) -> void {
  vm::uvm_unmap(pagetable, vm::TRAMPOLINE, 1, false);
  vm::uvm_unmap(pagetable, vm::TRAMFRAME, 1, false);
  vm::uvm_free(pagetable, sz);
}

auto free(struct process *p) -> void {
  if (p->trapframe) {
    vm::kevm.free(p->trapframe);
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

auto alloc_proc() -> std::optional<struct process *> {
  for (auto &p : proc_list) {
    lock::spin_acquire(&p.lock);
    if (p.status == proc_status::UNUSED) {
      p.pid = alloc_pid();
      p.status = proc_status::USED;

      p.trapframe = (struct trapframe *)vm::kevm.alloc().value();
      p.pagetable = alloc_pagetable(p);
      std::memset(&p.context, 0, sizeof(p.context));

      p.context.ra = (uint64_t)forkret;
      p.context.sp = p.kernel_stack + PGSIZE;

      return {&p};
    }
    lock::spin_release(&p.lock);
  }

  return {};
}

auto scheduler() -> void {
  auto *c = curr_cpu();
  c->proc = nullptr;

  while (true) {
    intr_on();

    auto found{false};

    for (auto &p : proc_list) {
      lock::spin_acquire(&p.lock);
      if (p.status == proc_status::RUNNABLE) {
        p.status = proc_status::RUNNING;
        c->proc = &p;
        swtch(&c->context, &p.context);

        c->proc = nullptr;
        found = true;
      }
      lock::spin_release(&p.lock);
    }

    if (found == false) {
      intr_on();
      asm volatile("wfi");
    }
  }
}

auto sched() -> void {
  auto *p = curr_proc();

  if (lock::spin_holding(&p->lock) == false) {
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
  lock::spin_acquire(&p->lock);
  p->status = proc_status::RUNNABLE;
  sched();
  lock::spin_release(&p->lock);
}

auto forkret() -> void {
  static bool first{true};

  lock::spin_release(&curr_proc()->lock);

  if (first) {
    fs::init(1);
    first = false;
    __sync_synchronize();
  }

  trap::user_ret();
}

auto user_init() -> void {
  auto *init_proc = alloc_proc().value();

  vm::uvm_first(init_proc->pagetable, (unsigned char *)initcode,
                sizeof(initcode));
  init_proc->sz = PGSIZE;

  init_proc->trapframe->epc = 0;
  init_proc->trapframe->sp = PGSIZE;

  std::strncpy(init_proc->name, "initcode", sizeof(init_proc->name));
  init_proc->cwd = fs::namei((char*)"/");
  init_proc->status = proc_status::RUNNABLE;

  lock::spin_release(&init_proc->lock);
}

auto sleep(void *chan, struct lock::spinlock &lock) -> void {
  auto *p = curr_proc();

  lock::spin_acquire(&p->lock);
  lock::spin_release(&lock);

  p->chan = chan;
  p->status = proc_status::SLEEPING;

  p->chan = nullptr;
  lock::spin_release(&p->lock);
  lock::spin_acquire(&lock);
}

auto wakeup(void *chan) -> void {
  for (auto &p : proc_list) {
    if (&p != curr_proc()) {
      lock::spin_acquire(&p.lock);
      if (p.status == proc_status::SLEEPING && p.chan == chan) {
        p.status = proc_status::RUNNABLE;
      }
      lock::spin_release(&p.lock);
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
    lock::spin_acquire(&p.lock);
    if (p.pid == pid) {
      p.killed = true;
      if (p.status == proc_status::SLEEPING) {
        p.status = proc_status::RUNNABLE;
      }
      lock::spin_release(&p.lock);
      return 0;
    }
    lock::spin_release(&p.lock);
  }
  return -1;
}

auto set_killed(struct process *proc) -> void {
  lock::spin_acquire(&proc->lock);
  proc->killed = true;
  lock::spin_release(&proc->lock);
}

auto get_killed(struct process *proc) -> bool {
  lock::spin_acquire(&proc->lock);
  auto rs = proc->killed;
  lock::spin_release(&proc->lock);
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
  auto opt_np = alloc_proc();
  if (!opt_np.has_value()) {
    return -1;
  }

  auto *p = curr_proc();
  auto *np = opt_np.value();

  if (vm::uvm_copy(p->pagetable, np->pagetable, p->sz) == false) {
    free(np);
    lock::spin_release(&np->lock);
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

  lock::spin_release(&np->lock);

  lock::spin_acquire(&wait_lock);
  np->parent = p;
  lock::spin_release(&wait_lock);

  lock::spin_acquire(&np->lock);
  np->status = proc_status::RUNNABLE;
  lock::spin_release(&np->lock);

  return static_cast<int32_t>(rs);
}

auto exit(int status) -> void {
  auto *p = curr_proc();
  if (p == init_proc) {
    fmt::panic("proc::exit: init exiting");
  }

  for (uint32_t fd {0}; fd < file::NOFILE; ++fd) {
    if (p->ofile[fd] != nullptr) {
      file::close(p->ofile[fd]);
      p->ofile[fd] = nullptr;
    }
  }

  log::begin_op();
  fs::iput(p->cwd);
  log::end_op();
  p->cwd = nullptr;

  lock::spin_acquire(&wait_lock);
  reparent(*p);
  wakeup(p->parent);

  p->xstate = status;
  p->status = proc_status::ZOMBIE;
  lock::spin_release(&wait_lock);

  sched();
  fmt::panic("proc::exit: zombie exit");
}

auto wait(uint64_t addr) -> int {
  lock::spin_acquire(&wait_lock);
  auto *p = curr_proc();

  while (true) {
    auto havekids{false};

    for (auto &cp : proc_list) {
      if (cp.parent == p) {
        lock::spin_acquire(&cp.lock);

        havekids = true;
        if (cp.status == ZOMBIE) {
          auto pid = cp.pid;
          if (addr != 0 && vm::copyout(p->pagetable, addr, (char *)&cp.xstate,
                                       sizeof(cp.xstate)) == false) {
            lock::spin_release(&cp.lock);
            lock::spin_release(&wait_lock);
            return -1;
          }
          free(&cp);
          lock::spin_release(&cp.lock);
          lock::spin_release(&wait_lock);
          return static_cast<int32_t>(pid);
        }

        lock::spin_release(&cp.lock);
      }
    }

    if (havekids == false || get_killed(p)) {
      lock::spin_release(&wait_lock);
      return -1;
    }

    sleep(p, wait_lock);
  }
}

auto either_copyout(bool user_dst, uint64_t dst, void *src, uint64_t len) -> int {
  auto *p = curr_proc();
  if (user_dst) {
    return vm::copyout(p->pagetable, dst, (char*)src, len);
  } 
  std::memmove((char*)dst, src, len);
  return 0;
}

auto either_copyin(void *dst, bool user_dst, uint64_t src, uint64_t len) -> int {
  auto *p =curr_proc();
  if (user_dst) {
    return vm::copyin(p->pagetable, (char*)dst, src, len);
  }
  std::memmove(dst, (char*)src, len);
  return 0;
}
}  // namespace proc
