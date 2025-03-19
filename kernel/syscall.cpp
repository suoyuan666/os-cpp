#include "syscall.h"

#include <cstdint>

#include "fmt.h"
#include "proc.h"

namespace syscall {

extern auto sys_fork() -> uint64_t;
extern auto sys_exit() -> uint64_t;
extern auto sys_wait() -> uint64_t;
extern auto sys_pipe() -> uint64_t;
extern auto sys_read() -> uint64_t;
extern auto sys_kill() -> uint64_t;
extern auto sys_exec() -> uint64_t;
extern auto sys_fstat() -> uint64_t;
extern auto sys_chdir() -> uint64_t;
extern auto sys_dup() -> uint64_t;
extern auto sys_getpid() -> uint64_t;
extern auto sys_sbrk() -> uint64_t;
extern auto sys_sleep() -> uint64_t;
extern auto sys_uptime() -> uint64_t;
extern auto sys_open() -> uint64_t;
extern auto sys_write() -> uint64_t;
extern auto sys_mknod() -> uint64_t;
extern auto sys_unlink() -> uint64_t;
extern auto sys_link() -> uint64_t;
extern auto sys_mkdir() -> uint64_t;
extern auto sys_close() -> uint64_t;


static uint64_t (*syscalls[])(void) = {
    sys_fork,  sys_exit,   sys_wait,  sys_pipe,  sys_read,   sys_kill,
    sys_exec,  sys_fstat,  sys_chdir, sys_dup,   sys_getpid, sys_sbrk,
    sys_sleep, sys_uptime, sys_open,  sys_write, sys_mknod,  sys_unlink,
    sys_link,  sys_mkdir,  sys_close,
};

auto syscall() -> void {
  auto *p = proc::curr_proc();

  auto num = p->trapframe->a7;
  if (num <= sizeof(syscalls)) {
    p->trapframe->a0 = syscalls[num]();
  } else {
    fmt::print("{} {} {} unknown syscall", p->pid, p->name, num);
  }
}
}//  namespace syscall
