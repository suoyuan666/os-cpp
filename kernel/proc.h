#pragma once
#include <cstdint>

#include "file.h"
#include "lock.h"

namespace proc {
constexpr uint32_t NPROC{64};
constexpr uint32_t NCPU{8};
constexpr uint32_t ROOT_ID{0};

struct context {
  uint64_t ra;
  uint64_t sp;

  // callee-saved
  uint64_t s0;
  uint64_t s1;
  uint64_t s2;
  uint64_t s3;
  uint64_t s4;
  uint64_t s5;
  uint64_t s6;
  uint64_t s7;
  uint64_t s8;
  uint64_t s9;
  uint64_t s10;
  uint64_t s11;
};

struct trapframe {
  /*   0 */ uint64_t kernel_satp;    // kernel page table
  /*   8 */ uint64_t kernel_sp;      // top of process's kernel stack
  /*  16 */ uint64_t kernel_trap;    // usertrap()
  /*  24 */ uint64_t epc;            // saved user program counter
  /*  32 */ uint64_t kernel_hartid;  // saved kernel tp
  /*  40 */ uint64_t ra;
  /*  48 */ uint64_t sp;
  /*  56 */ uint64_t gp;
  /*  64 */ uint64_t tp;
  /*  72 */ uint64_t t0;
  /*  80 */ uint64_t t1;
  /*  88 */ uint64_t t2;
  /*  96 */ uint64_t s0;
  /* 104 */ uint64_t s1;
  /* 112 */ uint64_t a0;
  /* 120 */ uint64_t a1;
  /* 128 */ uint64_t a2;
  /* 136 */ uint64_t a3;
  /* 144 */ uint64_t a4;
  /* 152 */ uint64_t a5;
  /* 160 */ uint64_t a6;
  /* 168 */ uint64_t a7;
  /* 176 */ uint64_t s2;
  /* 184 */ uint64_t s3;
  /* 192 */ uint64_t s4;
  /* 200 */ uint64_t s5;
  /* 208 */ uint64_t s6;
  /* 216 */ uint64_t s7;
  /* 224 */ uint64_t s8;
  /* 232 */ uint64_t s9;
  /* 240 */ uint64_t s10;
  /* 248 */ uint64_t s11;
  /* 256 */ uint64_t t3;
  /* 264 */ uint64_t t4;
  /* 272 */ uint64_t t5;
  /* 280 */ uint64_t t6;
};

struct user {
  uint32_t uid{0};
  uint32_t gid{0};
  class lock::spinlock lock{};
};

struct cpu {
  struct process *proc;
  struct context context;
  uint32_t noff;
  uint32_t intena;
};

extern struct cpu cpu;

enum class proc_status : char { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

struct process {
  class lock::spinlock lock{};

  char name[32];
  proc_status status;
  uint32_t pid;
  void *chan;
  bool killed;
  int xstate;
  struct user::user *user;

  struct process *parent;

  uint64_t kernel_stack;
  uint64_t sz;
  uint64_t *pagetable;
  struct trapframe *trapframe;
  struct context context;
  struct file::file *ofile[file::NOFILE];
  struct file::inode *cwd;
};

auto init() -> void;
auto user_init() -> void;
auto map_stack(uint64_t *kpt) -> void;
auto cpuid() -> uint32_t;
auto curr_proc() -> struct process *;
auto curr_cpu() -> struct cpu *;
auto alloc_pagetable(struct process &p) -> uint64_t *;
auto free_pagetable(uint64_t *pagetable, uint64_t sz) -> void;
auto sleep(void *chan, class lock::spinlock &lock) -> void;
auto wakeup(void *chan) -> void;
auto yield() -> void;
auto scheduler() -> void;
auto grow(int n) -> int;
auto fork() -> int32_t;
auto exit(int status) -> void;
auto wait(uint64_t addr) -> int;
auto kill(uint32_t pid) -> int;
auto get_killed(struct process *proc) -> bool;
auto set_killed(struct process *proc) -> void;
auto dump(struct process &p) -> void;
auto either_copyin(void *dst, bool user_dst, uint64_t src, uint64_t len) -> int;
auto either_copyout(bool user_dst, uint64_t dst, void *src, uint64_t len)
    -> int;
}  // namespace proc
