#include "pipe"

#include <cstdint>

#include "file"
#include "lock"
#include "proc"
#include "vm"

namespace file {
auto pipealloc(struct file **f0, struct file **f1) -> int {
  struct pipe *pi = nullptr;

  pi = nullptr;
  *f0 = *f1 = nullptr;
  if ((*f0 = alloc()) == nullptr || (*f1 = alloc()) == nullptr) {
    goto bad;
  }
  if ((pi = (struct pipe *)(alloc())) == nullptr) {
    goto bad;
  }
  pi->readopen = true;
  pi->writeopen = true;
  pi->nwrite = 0;
  pi->nread = 0;

  lock::spin_init(pi->lock, (char *)"pipe");
  (*f0)->type = ::file::file::FD_PIPE;
  (*f0)->readable = true;
  (*f0)->writable = false;
  (*f0)->pipe = pi;
  (*f1)->type = ::file::file::FD_PIPE;
  (*f1)->readable = false;
  (*f1)->writable = true;
  (*f1)->pipe = pi;
  return 0;

bad:
  if (pi) {
    vm::kfree(pi);
  }
  if (*f0) {
    close(*f0);
  }
  if (*f1) {
    close(*f1);
  }
  return -1;
}

auto pipeclose(struct pipe *pi, bool writable) -> void {
  lock::spin_acquire(&pi->lock);
  if (writable) {
    pi->writeopen = false;
    proc::wakeup(&pi->nread);
  } else {
    pi->readopen = false;
    proc::wakeup(&pi->nwrite);
  }
  if (pi->readopen == false && pi->writeopen == false) {
    spin_release(&pi->lock);
    vm::kfree(pi);
  } else
    spin_release(&pi->lock);
}

auto pipewrite(struct pipe *pi, uint64_t addr, int n) -> uint32_t {
  int i = 0;
  auto *pr = proc::curr_proc();

  spin_acquire(&pi->lock);
  while (i < n) {
    if (pi->readopen == false || proc::get_killed(pr)) {
      spin_release(&pi->lock);
      return -1;
    }
    if (pi->nwrite == pi->nread + PIPESIZE) {  // DOC: pipewrite-full
      proc::wakeup(&pi->nread);
      proc::sleep(&pi->nwrite, pi->lock);
    } else {
      char ch = 0;
      if (vm::copyin(pr->pagetable, &ch, addr + i, 1) == false) {
        break;
      }
      pi->data[pi->nwrite++ % PIPESIZE] = ch;
      i++;
    }
  }
  proc::wakeup(&pi->nread);
  spin_release(&pi->lock);

  return i;
}

auto piperead(struct pipe *pi, uint64_t addr, int n) -> uint32_t {
  int i = 0;
  auto *pr = proc::curr_proc();
  char ch = 0;

  spin_acquire(&pi->lock);
  while (pi->nread == pi->nwrite && pi->writeopen) {  // DOC: pipe-empty
    if (proc::get_killed(pr)) {
      spin_release(&pi->lock);
      return -1;
    }
    proc::sleep(&pi->nread, pi->lock);  // DOC: piperead-sleep
  }
  for (i = 0; i < n; i++) {  // DOC: piperead-copy
    if (pi->nread == pi->nwrite) {
      break;
    }
    ch = pi->data[pi->nread++ % PIPESIZE];
    if (vm::copyout(pr->pagetable, addr + i, &ch, 1) == false) {
      break;
    }
  }
  proc::wakeup(&pi->nwrite);
  spin_release(&pi->lock);
  return i;
}
}  // namespace file
