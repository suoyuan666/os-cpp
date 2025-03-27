#pragma once
#include <features.h>

#include "syscall_arch.h"

#ifndef __scc
#define __scc(X) ((long)(X))
typedef long syscall_arg_t;
#endif

#define SYS_fork 0
#define SYS_exit 1
#define SYS_wait 2
#define SYS_pipe 3
#define SYS_read 4
#define SYS_kill 5
#define SYS_exec 6
#define SYS_fstat 7
#define SYS_chdir 8
#define SYS_dup 9
#define SYS_getpid 10
#define SYS_sbrk 11
#define SYS_sleep 12
#define SYS_uptime 13
#define SYS_open 14
#define SYS_write 15
#define SYS_mknod 16
#define SYS_unlink 17
#define SYS_link 18
#define SYS_mkdir 29
#define SYS_close 20
#define SYS_setuid 21
#define SYS_setgid 22

hidden long __syscall_ret(unsigned long),
    __syscall_cp(syscall_arg_t, syscall_arg_t, syscall_arg_t, syscall_arg_t,
                 syscall_arg_t, syscall_arg_t, syscall_arg_t);

#define __syscall1(n, a) __syscall1(n, __scc(a))
#define __syscall2(n, a, b) __syscall2(n, __scc(a), __scc(b))
#define __syscall3(n, a, b, c) __syscall3(n, __scc(a), __scc(b), __scc(c))
#define __syscall4(n, a, b, c, d) \
  __syscall4(n, __scc(a), __scc(b), __scc(c), __scc(d))
#define __syscall5(n, a, b, c, d, e) \
  __syscall5(n, __scc(a), __scc(b), __scc(c), __scc(d), __scc(e))
#define __syscall6(n, a, b, c, d, e, f) \
  __syscall6(n, __scc(a), __scc(b), __scc(c), __scc(d), __scc(e), __scc(f))
#define __syscall7(n, a, b, c, d, e, f, g)                                  \
  __syscall7(n, __scc(a), __scc(b), __scc(c), __scc(d), __scc(e), __scc(f), \
             __scc(g))

#define __SYSCALL_NARGS_X(a, b, c, d, e, f, g, h, n, ...) n
#define __SYSCALL_NARGS(...) \
  __SYSCALL_NARGS_X(__VA_ARGS__, 7, 6, 5, 4, 3, 2, 1, 0, )
#define __SYSCALL_CONCAT_X(a, b) a##b
#define __SYSCALL_CONCAT(a, b) __SYSCALL_CONCAT_X(a, b)
#define __SYSCALL_DISP(b, ...) \
  __SYSCALL_CONCAT(b, __SYSCALL_NARGS(__VA_ARGS__))(__VA_ARGS__)

#define __syscall(...) __SYSCALL_DISP(__syscall, __VA_ARGS__)
#define syscall(...) __syscall_ret(__syscall(__VA_ARGS__))

#define __syscall_cp0(n) (__syscall_cp)(n,0,0,0,0,0,0)
#define __syscall_cp1(n,a) (__syscall_cp)(n,__scc(a),0,0,0,0,0)
#define __syscall_cp2(n,a,b) (__syscall_cp)(n,__scc(a),__scc(b),0,0,0,0)
#define __syscall_cp3(n,a,b,c) (__syscall_cp)(n,__scc(a),__scc(b),__scc(c),0,0,0)
#define __syscall_cp4(n,a,b,c,d) (__syscall_cp)(n,__scc(a),__scc(b),__scc(c),__scc(d),0,0)
#define __syscall_cp5(n,a,b,c,d,e) (__syscall_cp)(n,__scc(a),__scc(b),__scc(c),__scc(d),__scc(e),0)
#define __syscall_cp6(n,a,b,c,d,e,f) (__syscall_cp)(n,__scc(a),__scc(b),__scc(c),__scc(d),__scc(e),__scc(f))

#define __syscall_cp(...) __SYSCALL_DISP(__syscall_cp,__VA_ARGS__)
#define syscall_cp(...) __syscall_ret(__syscall_cp(__VA_ARGS__))
