#include "lock.h"

#ifndef ARCH_RISCV
#include "arch/riscv.h"
#define ARCH_RISCV
#endif

#include <fmt>

#include "proc.h"

namespace lock {

auto spinlock::holding() -> bool { return locked && cpu == proc::curr_cpu(); }

auto push_off() -> void {
  auto old = intr_get();

  intr_off();
  if (proc::curr_cpu()->noff == 0) {
    proc::curr_cpu()->intena = old;
  }
  proc::curr_cpu()->noff += 1;
}

auto pop_off() -> void {
  auto *c = proc::curr_cpu();
  if (intr_get()) {
    fmt::panic("pop_off - interruptible");
  }
  if (c->noff < 1) {
    fmt::panic("pop_off");
  }
  c->noff -= 1;
  if (c->noff == 0 && c->intena) {
    intr_on();
  }
}

auto spinlock::acquire() -> void {
  push_off();
  if (holding()) {
    fmt::panic("lock::acquire: already holding the lock");
  }

  // NOLINTBEGIN
  while (__sync_lock_test_and_set(&locked, true) != false);
  // NOLINTEND

  __sync_synchronize();

  cpu = proc::curr_cpu();
}

auto spinlock::release() -> void {
  if (holding() == false) {
    fmt::panic("lock::release: no lock");
  }

  cpu = nullptr;

  __sync_synchronize();

  // NOLINTBEGIN
  __sync_lock_release(&locked);
  // NOLINTEND

  pop_off();
}

auto sleeplock::acquire() -> void {
  lk.acquire();
  while (locked) {
    proc::sleep(this, lk);
  }
  locked = true;
  pid = proc::curr_proc()->pid;
  lk.release();
}

auto sleeplock::release() -> void {
  lk.acquire();
  locked = false;
  pid = 0;
  proc::wakeup(this);
  lk.release();
}

auto sleeplock::holding() -> bool {
  lk.acquire();
  auto rs = locked && (pid == proc::curr_proc()->pid);
  lk.release();
  return rs;
}
}  // namespace lock
