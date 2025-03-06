#include "file"

#include <cstdint>

#include "fmt"
#include "fs"
#include "lock"
#include "log"
#include "pipe"
#include "proc"
#include "vm"

namespace file {
struct devsw devsw[NDEV];

struct {
  struct lock::spinlock lock;
  struct file file[NFILE];
} ftable;

auto init() -> void { lock::spin_init(ftable.lock, (char*)"ftable"); }

auto alloc() -> struct file* {
  lock::spin_acquire(&ftable.lock);
  for (auto& file : ftable.file) {
    if (file.ref == 0) {
      file.ref = 1;
      lock::spin_release(&ftable.lock);
      return &file;
    }
  }
  lock::spin_release(&ftable.lock);
  return nullptr;
}

auto dup(struct file* f) -> struct file* {
  lock::spin_acquire(&ftable.lock);
  if (f->ref < 1) {
    fmt::panic("file::dup");
  }
  ++f->ref;
  lock::spin_release(&ftable.lock);
  return f;
}

auto close(struct file* f) -> void {
  lock::spin_acquire(&ftable.lock);
  if (f->ref < 1) {
    fmt::panic("file::close");
  }
  if (--f->ref > 0) {
    lock::spin_release(&ftable.lock);
    return;
  }

  auto ff = *f;
  f->ref = 0;
  f->type = file::FD_NONE;
  lock::spin_release(&ftable.lock);

  if (ff.type == file::FD_PIPE) {
    pipeclose(ff.pipe, ff.writable);
  } else if (ff.type == file::FD_INODE || ff.type == file::FD_DEVICE) {
    log::begin_op();
    fs::iput(ff.ip);
    log::end_op();
  }
}

auto stat(struct file* f, uint64_t addr) -> int {
  if (f->type == file::FD_INODE || f->type == file::FD_DEVICE) {
    struct stat st{};
    fs::ilock(f->ip);
    fs::stati(*f->ip, st);
    fs::iunlock(f->ip);
    if (vm::copyout(proc::curr_proc()->pagetable, addr, (char*)&st,
                    sizeof(st)) == false) {
      return -1;
    }
    return 0;
  }
  return -1;
}

auto read(struct file* f, uint64_t addr, int n) -> int {
  if (f->readable == 0) {
    return -1;
  }

  int rs = 0;
  if (f->type == file::FD_PIPE) {
    rs = piperead(f->pipe, addr, n);
  } else if (f->type == file::FD_DEVICE) {
    if (f->major < 0 || f->major >= static_cast<int16_t>(NDEV) ||
        !devsw[f->major].read) {
      return -1;
    }
    rs = devsw[f->major].read(true, addr, n);
  } else if (f->type == file::FD_INODE) {
    fs::ilock(f->ip);
    rs = static_cast<int>(fs::readi(f->ip, true, addr, f->off, n));
    if (rs > 0) {
      f->off += rs;
    }
    fs::iunlock(f->ip);
  } else {
    fmt::panic("file::read");
  }

  return rs;
}

auto write(struct file* f, uint64_t addr, int n) -> int {
  if (f->writable == false) {
    return -1;
  }

  auto rs{0};
  auto r{0};
  if (f->type == ::file::file::FD_PIPE) {
    rs = pipewrite(f->pipe, addr, n);
  } else if (f->type == T_DEVICE) {
    if (f->major < 0 || f->major >= static_cast<int16_t>(NDEV) || !devsw[f->major].write) {
      return -1;
    }
    rs = devsw[f->major].write(1, addr, n);
  } else if (f->type == ::file::file::FD_INODE) {
    int max = ((fs::MAXOPBLOCKS - 1 - 1 - 2) / 2) * fs::BSIZE;
    int i = 0;
    while (i < n) {
      int n1 = n - i;
      if (n1 > max) n1 = max;

      log::begin_op();
      fs::ilock(f->ip);
      if ((r = fs::writei(f->ip, true, addr + i, f->off, n1)) > 0) {
        f->off += r;
      }
      fs::iunlock(f->ip);
      log::end_op();

      if (r != n1) {
        // error from writei
        break;
      }
      i += r;
    }
    rs = (i == n ? n : -1);
  } else {
    fmt::panic("filewrite");
  }

  return rs;
}
}  // namespace file
