#include <array>
#include <cstdint>
#include <fmt>

#include "arch/riscv.h"
#include "file.h"
#include "fs.h"
#include "kernel/fs"
#include "loader.h"
#include "log.h"
#include "pipe.h"
#include "proc.h"
#include "syscall.h"
#include "vm.h"

namespace syscall {
auto fetch_addr(uint64_t addr, uint64_t *ip) -> bool {
  auto *proc = proc::curr_proc();
  if (addr >= proc->sz || addr + sizeof(uint64_t) > proc->sz) {
    return false;
  }
  if (vm::copyin(proc->pagetable, reinterpret_cast<char *>(ip), addr,
                 sizeof(*ip)) == false) {
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

auto check_permission_for_file(struct file::inode *ip, uint32_t mask) -> bool {
  auto *p = proc::curr_proc();
  if (ip->uid == p->user->uid) {
    if ((ip->mask_user & mask) == 0) {
      return false;
    }
  } else if (ip->gid == p->user->gid) {
    if ((ip->mask_group & mask) == 0) {
      return false;
    }
  } else {
    if ((ip->mask_other & mask) == 0) {
      return false;
    }
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
    if (p->ofile.at(fd) == nullptr) {
      p->ofile.at(fd) = f;
      return fd;
    }
  }

  return -1;
}

auto get_fd(uint32_t argu_no, struct file::file *&f) -> int {
  struct file::file *tf = nullptr;

  int fd = static_cast<int>(get_argu(argu_no));
  if (fd < 0 || fd >= static_cast<int>(file::NOFILE)) {
    return -1;
  }
  tf = proc::curr_proc()->ofile.at(fd);
  if (tf == nullptr) {
    return -1;
  }
  f = tf;
  return fd;
}

auto create(char *path, int type, int16_t major, int16_t minor)
    -> struct file::inode * {
  // char name[fs::DIRSIZ];
  std::array<char, fs::DIRSIZ> name{};

  auto *dp = fs::nameiparent(path, name.begin());
  if (dp == nullptr) {
    return nullptr;
  }

  fs::ilock(dp);

  auto *ip = fs::dir_lookup(dp, name.begin(), nullptr);
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

  ip = fs::ialloc(dp->dev, static_cast<int16_t>(type));
  if (ip == nullptr) {
    fs::iunlockput(dp);
    return nullptr;
  }

  fs::ilock(ip);
  const auto *p = proc::curr_proc();
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  ip->uid = p->user->uid;
  ip->gid = p->user->gid;
  ip->mask_user = static_cast<char>(7);
  ip->mask_group = static_cast<char>(7);
  ip->mask_other = static_cast<char>(7);
  fs::iupdate(ip);

  if (type == file::T_DIR) {
    const std::array<char, 2> curr_cwd{"."};
    const std::array<char, 3> prev_cwd{".."};
    if (fs::dir_link(ip, curr_cwd.begin(), ip->inum) < 0 ||
        fs::dir_link(ip, prev_cwd.begin(), ip->inum) < 0) {
      ip->nlink = 0;
      fs::iupdate(ip);
      fs::iunlockput(ip);
      fs::iunlockput(dp);
      return nullptr;
    }
  }

  if (fs::dir_link(dp, name.begin(), ip) < 0) {
    ip->nlink = 0;
    fs::iupdate(ip);
    fs::iunlockput(ip);
    fs::iunlockput(dp);
    return nullptr;
  }

  if (type == file::T_DIR) {
    dp->nlink++;
    fs::iupdate(dp);
  }

  fs::iunlockput(dp);

  return ip;
}

auto fail_work4exec(std::array<char *, loader::MAXARGV> &argv) -> void {
  for (auto &argu : argv) {
    if (argu == nullptr) break;
    vm::kfree(argu);
  }
}

auto sys_exec() -> uint64_t {
  std::array<char, file::MAXPATH> path{};
  std::array<char *, loader::MAXARGV> argv{};

  auto uargv = get_argu(1);
  uint64_t path_addr = get_argu(0);
  if (fetch_str(path_addr, path.begin(), file::MAXPATH) == false) {
    return -1;
  }

  uint32_t i{0};
  uint64_t uarg = 0;
  while (true) {
    if (i > argv.size()) {
      fail_work4exec(argv);
      return -1;
    }
    if (fetch_addr(uargv + sizeof(uint64_t) * i, &uarg) == false) {
      fail_work4exec(argv);
      return -1;
    }
    if (uarg == 0) {
      argv.at(i) = nullptr;
      break;
    }

    auto opt_argv = vm::kalloc();
    if (!opt_argv.has_value()) {
      fmt::panic("sys_exec: no memory");
    }
    argv.at(i) = reinterpret_cast<char *>(opt_argv.value());
    if (argv.at(i) == nullptr) {
      fail_work4exec(argv);
      return -1;
    }

    if (fetch_str(uarg, argv.at(i), PGSIZE) == false) {
      fail_work4exec(argv);
      return -1;
    }

    ++i;
  }

  auto *ip = fs::namei(path.begin());
  fs::ilock(ip);
  if (!check_permission_for_file(ip, 1U)) {
    fs::iunlock(ip);
    return -1;
  }

  auto rs = loader::exec(ip, path.begin(), argv.begin());

  if (rs == -1) {
    fmt::print("sys_exec: exec call failed\n");
  }
  for (auto &argu : argv) {
    if (argu == nullptr) break;
    vm::kfree(argu);
  }
  return rs;
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
  auto *p = proc::curr_proc();

  uint64_t fdarray = get_argu(0);
  if (file::pipealloc(&rf, &wf) < 0) {
    return -1;
  }

  auto fd0 = alloc_fd(rf);
  auto fd1 = alloc_fd(wf);
  if (fd0 < 0 || fd1 < 0) {
    if (fd0 >= 0) p->ofile.at(fd0) = nullptr;
    file::close(rf);
    file::close(wf);
    return -1;
  }
  // clang-format off
  if (!vm::copyout(p->pagetable, fdarray, 
                   reinterpret_cast<char *>(&fd0), sizeof(fd0))||
      !vm::copyout(p->pagetable, fdarray + sizeof(fd0),
                   reinterpret_cast<char *>(&fd1), sizeof(fd1))) {
    // clang-format on
    p->ofile.at(fd0) = nullptr;
    p->ofile.at(fd1) = nullptr;
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
  std::array<char, file::MAXPATH> path{};
  struct file::inode *ip = nullptr;
  auto *p = proc::curr_proc();

  log::begin_op();

  uint64_t addr = get_argu(0);
  if (fetch_str(addr, path.begin(), file::MAXPATH) == false) {
    log::end_op();
    return -1;
  }
  ip = fs::namei(path.begin());
  if (ip == nullptr) {
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

auto fd_alloc(struct file::file *&f) -> int {
  auto *p = proc::curr_proc();
  for (int fd = 0; fd < static_cast<int>(file::NOFILE); ++fd) {
    if (p->ofile.at(fd) == nullptr) {
      p->ofile.at(fd) = f;
      return fd;
    }
  }
  return -1;
}

auto sys_dup() -> uint64_t {
  struct file::file *f = nullptr;

  if (get_fd(0, f) < 0) {
    return -1;
  }
  auto fd = fd_alloc(f);
  if (fd < 0) {
    return -1;
  }

  file::dup(f);
  return fd;
}

auto sys_getpid() -> uint64_t { return proc::curr_proc()->pid; }

auto sys_sbrk() -> uint64_t {
  int n = static_cast<int>(get_argu(0));
  auto sz = proc::curr_proc()->sz;

  if (proc::grow(n) < 0) {
    return -1;
  }

  return sz;
}

auto sys_sleep() -> uint64_t { return 0; }
auto sys_uptime() -> uint64_t { return 0; }

auto sys_open() -> uint64_t {
  std::array<char, file::MAXPATH> path{};
  auto mode = static_cast<uint32_t>(get_argu(1));

  uint64_t addr = get_argu(0);
  if (fetch_str(addr, path.begin(), file::MAXPATH) == false) {
    return -1;
  }

  log::begin_op();

  struct file::inode *ip = nullptr;
  if (mode & file::O_CREATE) {
    ip = create(path.begin(), file::T_FILE, 0, 0);
    if (ip == nullptr) {
      log::end_op();
      return -1;
    }
  } else {
    ip = fs::namei(path.begin());
    if (ip == nullptr) {
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

  if (mode == file::O_RDONLY) {
    if (!check_permission_for_file(ip, 1U << 2U)) {
      fs::iunlockput(ip);
      log::end_op();
      return -1;
    }
  } else if (mode == file::O_WRONLY || mode == file::O_TRUNC) {
    if (!check_permission_for_file(ip, 1U << 1U)) {
      fs::iunlockput(ip);
      log::end_op();
      return -1;
    }
  } else if (mode == file::O_RDWR) {
    if (!check_permission_for_file(ip, (1U << 1U) | (1U << 2U))) {
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

auto sys_mknod() -> uint64_t {
  std::array<char, file::MAXPATH> path{};

  log::begin_op();

  auto major = static_cast<int16_t>(get_argu(1));
  auto minor = static_cast<int16_t>(get_argu(2));

  auto addr = get_argu(0);
  if (!fetch_str(addr, path.begin(), fs::MAXFILE)) {
    return -1;
  }
  auto *ip = create(path.begin(), file::T_DEVICE, major, minor);
  if (ip == nullptr) {
    return -1;
  }

  fs::iunlockput(ip);
  log::end_op();
  return 0;
}

auto sys_unlink() -> uint64_t { return 0; }
auto sys_link() -> uint64_t { return 0; }

auto sys_mkdir() -> uint64_t {
  std::array<char, file::MAXPATH> path{};

  log::begin_op();

  uint64_t addr = get_argu(0);
  if (fetch_str(addr, path.begin(), file::MAXPATH) == false) {
    log::end_op();
    return -1;
  }

  auto *ip = create(path.begin(), file::T_DIR, 0, 0);
  if (ip == nullptr) {
    log::end_op();
    return -1;
  }

  auto *p = proc::curr_proc();
  ip->uid = p->user->uid;
  ip->gid = p->user->gid;
  ip->mask_user = 7;
  ip->mask_group = 7;
  ip->mask_other = 7;

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
  proc::curr_proc()->ofile.at(fd) = nullptr;
  file::close(f);
  return 0;
}

auto sys_setuid() -> uint64_t {
  auto id = static_cast<uint32_t>(get_argu(0));
  auto *p = proc::curr_proc();
  p->user->lock.acquire();
  p->user->uid = id;
  p->user->lock.release();
  return 0;
}
auto sys_setgid() -> uint64_t {
  auto id = static_cast<uint32_t>(get_argu(0));
  auto *p = proc::curr_proc();
  p->user->lock.acquire();
  p->user->gid = id;
  p->user->lock.release();
  return 0;
}

}  // namespace syscall
