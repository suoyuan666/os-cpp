#define PGSIZE 4096  // bytes per page
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))
#define KERNBASE 0x80000000L
#define PHYSTOP (KERNBASE + 128 * 1024 * 1024)
#define TRAMPOLINE (MAXVA - PGSIZE)
#define TRAPFRAME (TRAMPOLINE - PGSIZE)
