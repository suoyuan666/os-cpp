#include <cstdint>
#include <cstring>

#include "arch/riscv"
#include "file"
#include "fs"
#include "kernel/fs"
#include "loader"
#include "log"
#include "pipe"
#include "proc"
#include "syscall"
#include "vm"

namespace syscall {
auto fetch_addr(uint64_t addr, uint64_t *ip) -> bool {
  auto *proc = proc::curr_proc();
  if (addr >= proc->sz || addr + sizeof(uint64_t) > proc->sz) {
    return false;
  }
  if (vm::copyin(proc->pagetable, (char *)ip, addr, sizeof(*ip)) == false) {
    return false;
  }
  return true;
}

auto fetch_str(uint64_t addr, char *buf, uint32_t len) -> bool {
  auto *proc = proc::curr_proc();
  if (vm::copyinstr(proc->pagetable, buf, addr, len) == false) {
    return false;
  }

  return true;
}

auto get_argu(uint32_t index) -> uint64_t {
  auto *p = proc::curr_proc();
  switch (index) {
    case 0:
      return p->trapframe->a0;
      break;
    case 1:
      return p->trapframe->a1;
      break;
    case 2:
      return p->trapframe->a2;
      break;
    case 3:
      return p->trapframe->a3;
      break;
    case 4:
      return p->trapframe->a4;
      break;
    case 5:
      return p->trapframe->a5;
      break;
    case 6:
      return p->trapframe->a6;
      break;
    default:
      break;
  }
  fmt::panic("syscall::get_argu: get arguments failed");
  return 0;
}

auto alloc_fd(struct file::file *&f) -> int {
  auto *p = proc::curr_proc();

  auto fd{0};
  for (; fd < static_cast<int>(file::NOFILE); ++fd) {
    if (p->ofile[fd] == nullptr) {
      p->ofile[fd] = f;
      return fd;
    }
  }

  return -1;
}

auto get_fd(uint32_t argu_no, struct file::file *&f) -> int {
  struct file::file *tf = nullptr;

  int fd = get_argu(argu_no);
  if (fd < 0 || fd >= static_cast<int>(file::NOFILE) ||
      (tf = proc::curr_proc()->ofile[fd]) == nullptr) {
    return -1;
  }
  f = tf;
  return fd;
}

auto create(char *path, int type, int16_t major, int16_t minor)
    -> struct file::inode * {
  char name[fs::DIRSIZ];

  auto *dp = fs::nameiparent(path, name);
  if (dp == nullptr) {
    return nullptr;
  }

  fs::ilock(dp);

  auto *ip = fs::dir_lookup(dp, name, nullptr);
  if (ip != nullptr) {
    fs::iunlockput(dp);
    fs::ilock(ip);
    if (type == file::T_FILE &&
        (ip->type == file::T_FILE || ip->type == file::T_DEVICE)) {
      return ip;
    }
    fs::iunlockput(ip);
    return nullptr;
  }

  if ((ip = fs::ialloc(dp->dev, type)) == nullptr) {
    fs::iunlockput(dp);
    return nullptr;
  }

  fs::ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  fs::iupdate(ip);

  if (type == file::T_DIR) {
    if (fs::dir_link(ip, (char *)".", ip->inum) < 0 ||
        fs::dir_link(ip, (char *)"..", ip->inum) < 0) {
      goto fail;
    }
  }

  if (fs::dir_link(dp, name, ip->inum) < 0) {
    goto fail;
  }

  if (type == file::T_DIR) {
    dp->nlink++;
    fs::iupdate(dp);
  }

  fs::iunlockput(dp);

  return ip;

fail:
  ip->nlink = 0;
  fs::iupdate(ip);
  fs::iunlockput(ip);
  fs::iunlockput(dp);
  return nullptr;
}

auto sys_exec() -> uint64_t {
  char path[file::MAXPATH]{};
  char *argv[loader::MAXARGV]{};

  auto uargv = get_argu(1);
  uint64_t path_addr = get_argu(0);
  if (fetch_str(path_addr, path, file::MAXPATH) == false) {
    return -1;
  }
  std::memmove(argv, 0, sizeof(argv));

  uint32_t i{0};
  uint64_t uarg = 0;
  while (true) {
    if (i > sizeof(argv) / sizeof(char *)) {
      goto bad;
    }
    if (fetch_addr(uargv + sizeof(uint64_t) * i, &uargv) == false) {
      goto bad;
    }
    if (uarg == 0) {
      argv[i] = nullptr;
      break;
    }

    argv[i] = (char *)vm::kalloc().value();
    if (argv[i] == nullptr) {
      goto bad;
    }

    if (fetch_str(uarg, argv[i], PGSIZE) == false) {
      goto bad;
    }
  }

  auto rs = loader::exec(path, argv);
  for (i = 0; i < sizeof(argv) / sizeof(char *) && argv[i] != nullptr; ++i) {
    vm::kfree(argv[i]);
  }
  return rs;

bad:
  for (i = 0; i < sizeof(argv) / sizeof(char *) && argv[i] != nullptr; ++i) {
    vm::kfree(argv[i]);
  }
  return -1;
}

auto sys_fork() -> uint64_t { return proc::fork(); }

auto sys_exit() -> uint64_t {
  int status = static_cast<int>(get_argu(0));
  proc::exit(status);
  return 0;
}

auto sys_wait() -> uint64_t {
  uint64_t p = get_argu(0);
  return proc::wait(p);
}

auto sys_pipe() -> uint64_t {
  struct file::file *rf = nullptr;
  struct file::file *wf = nullptr;
  int fd0 = 0, fd1 = 0;
  auto *p = proc::curr_proc();

  uint64_t fdarray = get_argu(0);
  if (file::pipealloc(&rf, &wf) < 0) {
    return -1;
  }
  fd0 = -1;
  if ((fd0 = alloc_fd(rf)) < 0 || (fd1 = alloc_fd(wf)) < 0) {
    if (fd0 >= 0) p->ofile[fd0] = 0;
    file::close(rf);
    file::close(wf);
    return -1;
  }
  if (vm::copyout(p->pagetable, fdarray, (char *)&fd0, sizeof(fd0)) == false ||
      vm::copyout(p->pagetable, fdarray + sizeof(fd0), (char *)&fd1,
                  sizeof(fd1)) == false) {
    p->ofile[fd0] = nullptr;
    p->ofile[fd1] = nullptr;
    file::close(rf);
    file::close(wf);
    return -1;
  }
  return 0;
}

auto sys_read() -> uint64_t {
  uint64_t addr = get_argu(1);
  int size = static_cast<int>(get_argu(2));

  struct file::file *f = nullptr;
  if (get_fd(0, f) == -1) {
    return -1;
  }

  return file::read(f, addr, size);
}

auto sys_kill() -> uint64_t {
  int pid = static_cast<int>(get_argu(0));
  return proc::kill(pid);
}

auto sys_fstat() -> uint64_t {
  struct file::file *f = nullptr;
  uint64_t st = get_argu(1);

  if (get_fd(0, f) == -1) {
    return -1;
  }
  return file::stat(f, st);
}

auto sys_chdir() -> uint64_t {
  char path[file::MAXPATH];
  struct file::inode *ip;
  auto *p = proc::curr_proc();

  log::begin_op();

  uint64_t addr = get_argu(0);
  if (fetch_str(addr, path, file::MAXPATH) == false) {
    log::end_op();
    return -1;
  }
  if ((ip = fs::namei(path)) == nullptr) {
    log::end_op();
    return -1;
  }
  fs::ilock(ip);
  if (ip->type != file::T_DIR) {
    fs::iunlockput(ip);
    log::end_op();
    return -1;
  }
  fs::iunlock(ip);
  fs::iput(p->cwd);
  log::end_op();
  p->cwd = ip;
  return 0;
}

auto sys_dup() -> uint64_t {}
auto sys_getpid() -> uint64_t { return proc::curr_proc()->pid; }
auto sys_sbrk() -> uint64_t {
  int n = static_cast<int>(get_argu(0));
  auto sz = proc::curr_proc()->sz;

  if (proc::grow(n) < 0) {
    return -1;
  }

  return sz;
}
auto sys_sleep() -> uint64_t {}
auto sys_uptime() -> uint64_t {}
auto sys_open() -> uint64_t {
  char path[file::MAXPATH]{};
  int mode = static_cast<int>(get_argu(1));

  uint64_t addr = get_argu(0);
  if (fetch_str(addr, path, file::MAXPATH) == false) {
    return -1;
  }

  log::begin_op();

  struct file::inode *ip = nullptr;
  if (mode & file::O_CREATE) {
    ip = create(path, file::T_FILE, 0, 0);
    if (ip == nullptr) {
      log::end_op();
      return -1;
    }
  } else {
    if ((ip = fs::namei(path)) == nullptr) {
      log::end_op();
      return -1;
    }
    fs::ilock(ip);
    if (ip->type == file::T_DIR && mode != file::O_RDONLY) {
      fs::iunlockput(ip);
      log::end_op();
      return -1;
    }
  }

  if (ip->type == file::T_DEVICE &&
      (ip->major < 0 || ip->major >= static_cast<int16_t>(file::NDEV))) {
    fs::iunlockput(ip);
    log::end_op();
    return -1;
  }

  auto *f = file::alloc();
  if (f == nullptr) {
    fs::iunlockput(ip);
    log::end_op();
    return -1;
  }

  auto fd = alloc_fd(f);
  if (fd == -1) {
    file::close(f);
    fs::iunlockput(ip);
    log::end_op();
    return -1;
  }

  if (ip->type == file::T_DEVICE) {
    f->type = file::file::FD_DEVICE;
    f->major = ip->major;
  } else {
    f->type = file::file::FD_INODE;
    f->off = 0;
  }
  f->ip = ip;
  f->readable = !(mode & file::O_WRONLY);
  f->writable = (mode & file::O_WRONLY) || (mode & file::O_RDWR);

  if ((mode & file::O_TRUNC) && ip->type == file::T_FILE) {
    fs::itrunc(ip);
  }

  fs::iunlock(ip);
  log::end_op();

  return fd;
}
auto sys_write() -> uint64_t {
  uint64_t addr = get_argu(1);
  int size = static_cast<int>(get_argu(2));

  struct file::file *f = nullptr;
  if (get_fd(0, f) == -1) {
    return -1;
  }

  return file::write(f, addr, size);
}
auto sys_mknod() -> uint64_t {}
auto sys_unlink() -> uint64_t {}
auto sys_link() -> uint64_t {}
auto sys_mkdir() -> uint64_t {
  char path[file::MAXPATH];
  struct file::inode *ip;

  log::begin_op();

  uint64_t addr = get_argu(0);
  if (fetch_str(addr, path, file::MAXPATH) == false) {
    log::end_op();
    return -1;
  }
  if((ip = create(path, T_DIR, 0, 0)) == 0){
    log::end_op();
    return -1;
  }
  fs::iunlockput(ip);
  log::end_op();
  return 0;
}
auto sys_close() -> uint64_t {
  struct file::file *f = nullptr;
  auto fd = get_fd(0, f);
  if (fd == -1) {
    return -1;
  }
  proc::curr_proc()->ofile[fd] = nullptr;
  file::close(f);
  return 0;
}

}  // namespace syscall
