# proc

proc 是 os-cpp 的进程相关的抽象，其中包括进程的结构体抽象定义，以及相关的函数，比如 `fork` 之类的。

```cpp
enum class proc_status : char { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

struct process {
  class lock::spinlock lock{};   // 用于使用互斥锁防止该进程的信息处于数据竞争的状态

  char name[32];                // 进程的名称
  enum proc_status status;      // 进程的状态，状态种类在上面的 `enum proc_status` 被定义
  uint32_t pid;                 // 进程编号，从 1 开始
  void *chan;                   // 用于配合 sleep-wakeup 锁机制使用，该字段会存储条件变量，之后调用 wakeup 的时候会找到条件变量一致的再唤醒
  bool killed;                  // 指代是否应该被 kill
  int xstate;                   // 进程推出时的状态码 

  struct process *parent;       // 父进程

  uint64_t kernel_stack;;       // 内核栈区的地址
  uint64_t sz;                  // 进程所占的地址空间大小
  uint64_t *pagetable;          // 页表地址
  struct trapframe *trapframe;  // 用户态与内核态切换时，需要保存一些信息，这是保存信息区域的地址
  struct context context;       // 用于进程间上下文切换
  struct file::file *ofile[file::NOFILE];   // 进程所打开的文件
  struct file::inode *cwd;                  // 进程当前所在的目录
};
```

上面这段结构体定义就是 os-cpp 对进程的抽象，并且已经附上了注释

## 初始化
### 进程模块的初始化

```cpp
auto init() -> void {
  for (auto &proc : proc_list) {
    proc.status = proc_status::UNUSED;
    proc.kernel_stack =
        vm::KSTACK((int)((uint64_t)&proc - (uint64_t)&proc_list[0]));
  }
}
```

初始化函数很简单，遍历每个进程数组，设置好状态和内核栈区的地址。之所以选择了直接线性遍历的方式，是因为进程列表的个数很小:

```cpp
constexpr uint32_t NPROC{64};
```

是的，默认就这么点内存数，现在打开一个使用 Linux kernel 的发行版，登陆到桌面环境里，可能都不止一百个进程在运行中。os-cpp 并没有准备上来就支持那么多的进程数

> 在 Linux 中，可以通过下边这段指令查看当前用户的最大进程数
>
> ```bash
> ulimit -u
> ```

### 第一个进程的初始化

在大部分初始化都完成后，就要开始 init 进程的初始化，它负责初始化 init 进程的脚手架，这个脚手架负责启动 init 进程。其函数如下:

```cpp
auto user_init() -> void {
  auto *p = alloc_proc();
  init_proc = p;

  vm::uvm_first(p->pagetable, (unsigned char *)initcode, sizeof(initcode));
  p->sz = PGSIZE;

  p->trapframe->epc = 0;
  p->trapframe->sp = PGSIZE;

  std::strncpy(p->name, "initcode", sizeof(p->name));
  p->cwd = fs::namei((char *)"/");
  p->status = proc_status::RUNNABLE;

  lock::spin_release(&p->lock);
}
```

其中，`initcode` 就是这个脚手架，和 init 等程序不同的是，它被写死在内核代码中，而不是作为一个二进制可执行文件而存在。

```cpp
/* $ riscv64-linux-gnu-readelf -x .text build/utils/initcode
 *
 * Hex dump of section '.text':
 *   0x00000000 17050000 13054502 97050000 93854502 ......E.......E.
 *   0x00000010 93086000 73000000 93081000 73000000 ..`.s.......s...
 *   0x00000020 eff09fff 2f696e69 74000000 24000000 ..../init...$...
 *   0x00000030 00000000 00000000 00000000          ............
*/

const unsigned char initcode[] = {
    0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02, 0x97, 0x05, 0x00, 0x00,
    0x93, 0x85, 0x35, 0x02, 0x93, 0x08, 0x60, 0x00, 0x73, 0x00, 0x00, 0x00,
    0x93, 0x08, 0x10, 0x00, 0x73, 0x00, 0x00, 0x00, 0xef, 0xf0, 0x9f, 0xff,
    0x2f, 0x69, 0x6e, 0x69, 0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
```

注释写出了这段数据是如何得出的，即使用 `readelf` 查看 initcode 的 .text 段的数据。


