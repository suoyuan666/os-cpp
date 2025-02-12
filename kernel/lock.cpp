#include "lock"

#ifndef ARCH_RISCV
#include "arch/riscv"
#define ARCH_RISCV
#endif

#include "fmt"
#include "proc"

namespace lock {
auto spin_holding(struct spinlock *lock) -> bool {
  return lock->locked && lock->cpu == proc::curr_cpu();
}

auto spin_init(struct spinlock &lock, char *name) -> void {
  lock.name = name;
  lock.locked = false;
  lock.cpu = nullptr;
}

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

auto spin_acquire(struct spinlock *lk) -> void {
  push_off();
  if (spin_holding(lk)) {
    fmt::panic("lock::acquire: already holding the lock");
  }
  while (__sync_lock_test_and_set(&lk->locked, true) != 0) {
    ;
  }
  __sync_synchronize();

  lk->cpu = proc::curr_cpu();
}

auto spin_release(struct spinlock *lk) -> void {
  if (spin_holding(lk) == false) {
    fmt::panic("lock::release: no lock");
  }

  lk->cpu = nullptr;

  __sync_synchronize();

  __sync_lock_release(&lk->locked);

  pop_off();
}

auto sleep_init(struct sleeplock &lock, char *name) -> void {
  spin_init(lock.lk, (char*)"sleep lock");
  lock.name = name;
  lock.locked = false;
  lock.pid = 0;
}

auto sleep_acquire(struct sleeplock &lock) -> void {
  spin_acquire(&lock.lk);
  while (lock.locked) {
    proc::sleep(&lock, lock.lk);
  }
  lock.locked = true;
  lock.pid = proc::curr_proc()->pid;
  lock::spin_release(&lock.lk);
}

auto sleep_release(struct sleeplock &lock) -> void {
  spin_acquire(&lock.lk);
  lock.locked = false;
  lock.pid = 0;
  proc::wakeup(&lock);
  spin_release(&lock.lk);
}

auto sleep_holding(struct sleeplock &lock) -> bool {
  spin_acquire(&lock.lk);
  auto rs = lock.locked && (lock.pid == proc::curr_proc()->pid);
  spin_release(&lock.lk);
  return rs;
}
}  // namespace lock
