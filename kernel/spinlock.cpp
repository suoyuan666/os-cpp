#include "spinlock"

#include "arch/riscv"
#include "fmt"
#include "proc"

namespace spinlock {
auto holding(struct lock *lock) -> bool {
  return lock->locked && lock->cpu == proc::curr_cpu();
}

auto init(struct lock &lock, char *name) -> void {
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

auto acquire(struct lock *lk) -> void {
  push_off();
  if (holding(lk)) {
    fmt::panic("spinlock::acquire: already holding the lock");
  }
  while (__sync_lock_test_and_set(&lk->locked, true) != 0) {
    ;
  }
  __sync_synchronize();

  lk->cpu = proc::curr_cpu();
}

auto release(struct lock *lk) -> void {
  if (holding(lk) == false) {
    fmt::panic("spinlock::release: no lock");
  }

  lk->cpu = nullptr;

  __sync_synchronize();

  __sync_lock_release(&lk->locked);

  pop_off();
}

}  // namespace spinlock
