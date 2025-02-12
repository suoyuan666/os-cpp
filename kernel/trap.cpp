#include <cstdint>

#ifndef ARCH_RISCV
#include "arch/riscv"
#define ARCH_RISCV
#endif

#include "fmt"
#include "lock"
#include "plic"
#include "proc"
#include "syscall"
#include "trap"
#include "uart"
#include "virtio_disk"
#include "vm"

extern "C" auto kernelvec() -> void;
extern "C" char trampoline[], uservec[], userret[];

namespace trap {
using proc::get_killed;

struct lock::spinlock tickslock;
uint32_t ticks;

auto devintr() -> int;

auto init() -> void { lock::spin_init(tickslock, (char *)"time"); }

auto inithart() -> void { w_stvec((uint64_t)kernelvec); }

auto user_trap() -> void {
  if ((r_sstatus() & SSTATUS_SPP) != 0) {
    fmt::panic("usertrap: not from user mode");
  }

  int which_dev = 0;

  w_stvec((uint64_t)kernelvec);

  auto *p = proc::curr_proc();
  p->trapframe->epc = r_sepc();

  if (r_scause() == 8) {
    if (get_killed(p)) {
      proc::exit(-1);
    }
    p->trapframe->epc += 4;
    intr_on();
    syscall::syscall();
  } else if ((which_dev = devintr()) != 0) {
    // ok
  } else {
    // clang-format off
    fmt::print("usertrap(): unexpected scause 0x{x} pid={}\n", r_scause(), p->pid);
    fmt::print("            sepc=0x{x} stval=0x{x}\n", r_sepc(), r_stval());
    // clang-format on
    proc::set_killed(p);
  }

  if (proc::get_killed(p)) {
    proc::exit(-1);
  }

  if (which_dev == 2) {
    proc::yield();
  }

  user_ret();
}

auto user_ret() -> void {
  auto *p = proc::curr_proc();

  intr_off();
  uint64_t trampoline_uservec = vm::TRAMPOLINE + (uservec - trampoline);

  w_stvec(trampoline_uservec);

  p->trapframe->kernel_satp = r_satp();
  p->trapframe->kernel_sp = p->kernel_stack + PGSIZE;
  p->trapframe->kernel_trap = (uint64_t)user_trap;

  auto x = r_sstatus();
  x &= ~SSTATUS_SPP;
  x |= SSTATUS_SPIE;
  w_sstatus(x);

  w_sepc(p->trapframe->epc);

  uint64_t satp = MAKE_SATP(p->pagetable);

  uint64_t trampoline_userret = vm::TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64_t))trampoline_userret)(satp);
}

extern "C" auto kerneltrap() -> void {
  int which_dev = 0;
  uint64_t sepc = r_sepc();
  uint64_t sstatus = r_sstatus();
  uint64_t scause = r_scause();

  if ((sstatus & SSTATUS_SPP) == 0) {
    fmt::panic("kerneltrap: not from supervisor mode");
  }
  if (intr_get() != 0) {
    fmt::panic("kerneltrap: interrupts enabled");
  }

  if ((which_dev = devintr()) == 0) {
    fmt::print("scause={} sepc=0x{} stval=0x{}\n", scause, r_sepc(), r_stval());
    fmt::panic("trap::kerneltrap");
  }

  if (which_dev == 2 && proc::curr_proc() != nullptr) {
    proc::yield();
  }

  w_sepc(sepc);
  w_sstatus(sstatus);
}

void clockintr() {
  if (proc::cpuid() == 0) {
    spin_acquire(&tickslock);
    ticks++;
    proc::wakeup(&ticks);
    spin_release(&tickslock);
  }

  w_stimecmp(r_time() + 1000000);
}

auto devintr() -> int {
  uint64_t scause = r_scause();

  if (scause == 0x8000000000000009L) {
    int irq = plic::plic_claim();

    if (irq == vm::UART_IRQ) {
      uart::intr();
    } else if (irq == vm::VIRTIO_IRQ) {
      virtio_disk::virtio_disk_intr();
    } else if (irq) {
      fmt::print("unexpected interrupt irq={}\n", irq);
    }

    if (irq) {
      plic::plic_complete(irq);
    }

    return 1;
  }
  if (scause == 0x8000000000000005L) {
    clockintr();
    return 2;
  }
  return 0;
}
}  // namespace trap
