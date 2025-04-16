#include <cstdint>
#include <fmt>

#ifndef ARCH_RISCV
#include "arch/riscv.h"
#define ARCH_RISCV
#endif

#include "lock.h"
#include "plic.h"
#include "proc.h"
#include "syscall.h"
#include "trap.h"
#include "uart.h"
#include "virtio.h"
#include "vm.h"

extern "C" auto kernelvec() -> void;
extern "C" char trampoline[], uservec[], userret[];

namespace trap {

class lock::spinlock tickslock{};
uint32_t ticks;

auto devintr() -> int;

auto inithart() -> void { w_stvec(reinterpret_cast<uint64_t>(kernelvec)); }

auto user_trap() -> void {
  if ((r_sstatus() & SSTATUS_SPP) != 0) {
    fmt::panic("user_trap: not from user mode");
  }

  int which_dev{0};

  w_stvec(reinterpret_cast<uint64_t>(kernelvec));

  auto *p = proc::curr_proc();
  p->trapframe->epc = r_sepc();

  if (r_scause() == 8) {
    if (proc::get_killed(p)) {
      proc::exit(-1);
    }
    p->trapframe->epc += 4;
    intr_on();
    syscall::syscall();
  } else {
    which_dev = devintr();
    if (which_dev == 0) {
      fmt::print(
          "usertrap(): unexpected scause 0x{x}, sepc=0x{x}, stval=0x{x}\n",
          r_scause(), r_sepc(), r_stval());
      fmt::print("            pid: {}, name: {}\n", p->pid, p->name);
      proc::set_killed(p);
    }
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
  auto trampoline_uservec{vm::TRAMPOLINE + (uservec - trampoline)};

  w_stvec(trampoline_uservec);

  p->trapframe->kernel_satp = r_satp();
  p->trapframe->kernel_sp = p->kernel_stack + PGSIZE;
  p->trapframe->kernel_trap = reinterpret_cast<uint64_t>(user_trap);
  p->trapframe->kernel_hartid = r_tp();

  auto x = r_sstatus();
  x &= ~SSTATUS_SPP;
  x |= SSTATUS_SPIE;
  w_sstatus(x);

  w_sepc(p->trapframe->epc);

  uint64_t satp = MAKE_SATP(p->pagetable);

  uint64_t trampoline_userret = vm::TRAMPOLINE + (userret - trampoline);
  (reinterpret_cast<void (*)(uint64_t)>(trampoline_userret))(satp);
}

extern "C" auto kerneltrap() -> void {
  if (intr_get() != 0) {
    fmt::panic("kerneltrap: interrupts enabled");
  }

  auto sepc = r_sepc();
  auto sstatus = r_sstatus();
  auto scause = r_scause();

  if ((sstatus & SSTATUS_SPP) == 0) {
    fmt::panic("kerneltrap: not from supervisor mode");
  }

  auto dev = devintr();
  if (dev == 0) {
    fmt::print("scause={} sepc=0x{x} stval=0x{x}\n", scause, r_sepc(),
               r_stval());
    fmt::panic("trap::kerneltrap");
  }

  if (dev == 2 && proc::curr_proc() != nullptr) {
    proc::yield();
  }

  w_sepc(sepc);
  w_sstatus(sstatus);
}

void clockintr() {
  if (proc::cpuid() == 0) {
    tickslock.acquire();
    ticks++;
    proc::wakeup(&ticks);
    tickslock.release();
  }

  w_stimecmp(r_time() + 1000000);
}

auto devintr() -> int {
  auto scause = r_scause();

  if (scause == 0x8000000000000009L) {
    auto irq = plic::plic_claim();

    if (irq == static_cast<int>(vm::UART_IRQ)) {
      uart::intr();
    } else if (irq == static_cast<int>(vm::VIRTIO_IRQ)) {
      virtio::virtio_disk_intr();
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
