# kernel

os-cpp 的 kernel 是一个十分简单的 kernel，基本不存在什么用来性能优化的算法（未来会改进这一点），各个模块基本都用 C++ 的 namespace 隔离。

os-cpp 面向 [qemu 的 virt](https://www.qemu.org/docs/master/system/riscv/virt.html) 平台编写的。

qemu 的参数 `-bios none` 会阻止加载 [OpenSBI](https://gitlab.com/qemu-project/opensbi)，OpenSBI 会作为 BIOS 执行一些初始化工作，而这些初始化工作全部由 kernel 完成

## kernel 的初始化

### 栈指针的初始化

去除了 BIOS 的加载后，qemu 会直接从 `0x80000000` 地址开始执行，所以存在一个 start.S

```asm
.section .text
.global _start
_start:
    la sp, stack0
    li a0, 1024*4
    csrr a1, mhartid
    addi a1, a1, 1
    mul a0, a0, a1
    add sp, sp, a0
    call start
spin:
    j spin
```

对于 RISC-V 来说，机器启动后的 CPU 寄存器状态，除了 PC 的值不会是 0 之外，其他寄存器的值都是 0。所以这里要初始化 sp 栈指针寄存器。

### CPU 执行状态切换以及其他 CPU 相关的初始化

初始化了 sp 寄存器之后，就会调用 start 函数，该函数会继续做其他初始化工作

其他的初始化工作都被定义在 **main.cpp** 文件里

```cpp
extern "C" void start() {
  auto mstatus = r_mstatus();
  mstatus &= ~MSTATUS_MPP_MASK;
  mstatus |= MSTATUS_MPP_S;

  w_mstatus(mstatus);

  w_mepc((uint64_t)main);

  w_satp(0);

  w_medeleg(0xffff);
  w_mideleg(0xffff);
  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

  w_pmpaddr0(0x3fffffffffffffULL);
  w_pmpcfg0(0xf);

  timerinit();

  auto id = r_mhartid();
  w_tp(id);

  asm volatile("mret");
}
```

`extern "C"` 是为了保证 `start` 在编译时不会经历 C++ 的符号修饰部分，`_start` 中的 `call start` 会跳转到 `start` 这里执行

`start` 函数中先通过 `r_mstatus` 函数将当前 mstatus 寄存器的值读出来，之后将这个值设置为 supervisor 模式对应的值，之后调用 `w_mstatus` 函数将其写入到 mstatus。

> kernel 中充斥着大量的类似 `r_mstatus` 和 `w_mstatus` 的函数，这分别代表着读取 `mstatus` 寄存器的值和改变 `mstatus` 寄存器的值。
>
> ```c
> static inline auto r_mstatus() -> uint64_t {
>  uint64_t rs{};
>  asm volatile("csrr %0, mstatus" : "=r"(rs));
>  return rs;
> };
> static inline auto w_mstatus(uint64_t value) -> void {
>  asm volatile("csrw mstatus, %0" : : "r"(value));
> };
> ```
>
> 类似上边这样的函数写的那样，通过 C++ 提供的[内联汇编](https://en.cppreference.com/w/cpp/language/asm)完成了对寄存器的读写

通过 `w_mepc` 将 `main` 函数的地址写入到 mepc 寄存器，这和写入到 mstatus 寄存器的行为都有一个共同之处，那就是都需要等到最后的 `mret` 指令调用后才能起作用。

并不是写入了 `mstatus` 寄存器就能改变当前的运行模式，这需要 `mret` 指令的配合。`mret` 类似于 `ret`，不同于用户态的 `ret`，`mret`/`sret` 都会将当前的运行模式进行一次切换，并且将 PC 的值更改，而之前的运行模式是 `mstatus`/`sstatus` 寄存器保存的，而之前的 PC 的值也是 `mepc`/`sepc` 寄存器保存的。

从这些寄存器和指令的前缀可以看出，RISC-V 的模式分为三种，分别是 machine、supervisor 和 user。machine 和 supervisor 模式的指令和寄存器都需要有 `m` 和 `s` 为前缀。系统启动时会先处于 machine 模式，而执行完 `start` 之后，运行模式就会变成 supervisor 模式，并继续执行内核初始化的部分。

### 内核的初始化

```cpp
auto main() -> void {
  if (proc::cpuid() == 0) {
    console::init();
    fmt::print("\n");
    fmt::print_log(fmt::log_level::INFO, "console init successful\n");
    vm::kinit();
    vm::init();
    vm::inithart();
    fmt::print_log(fmt::log_level::INFO, "vm init successful\n");
    proc::init();
    fmt::print_log(fmt::log_level::INFO, "proc init successful\n");
    trap::init();
    trap::inithart();
    fmt::print_log(fmt::log_level::INFO, "trap init successful\n");
    plic::init();
    plic::inithart();
    fmt::print_log(fmt::log_level::INFO, "plic init successful\n");
    bio::init();
    fs::iinit();
    file::init();
    fmt::print_log(fmt::log_level::INFO, "file system init successful\n");
    virtio_disk::init();
    fmt::print_log(fmt::log_level::INFO, "disk init successful\n");
    proc::user_init();
    __sync_synchronize();
    started = true;
  } else {
    while (started == false);
    __sync_synchronize();
    fmt::print("hart {} starting\n", proc::cpuid());
    vm::inithart();
    trap::inithart();
    plic::inithart();
  }

  proc::scheduler();
}
```

这里初始化的顺序并不是唯一的，不过 UART 的初始化包含在 console 的初始化中了，也可以将 UART 初始化提出来，因为 fmt 直接使用了 UART 中的 putc 进行输出工作。

总的来说，经历了一系列的初始化工作后，就会调用 `proc::scheduler()` 进程调度器，因为 `proc::user_init()` 会初始化一个 init 进程的脚手架，这个脚手架的代码直接写死在了 proc.cpp 代码中

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

initcode 只做一件事，那就是启动 init 程序，initcode 的代码在 utils 中，也就是 initcode.S

```asm
# exec(init, argv)
.globl start
start:
        la a0, init
        la a1, argv
        li a7, 6
        ecall

# for(;;) exit();
exit:
        li a7, 1
        ecall
        jal exit

# char init[] = "/init\0";
init:
  .string "/init\0"

# char *argv[] = { init, 0 };
.p2align 2
argv:
  .quad init
  .quad 0
```

所以进程调度器可以找到这个脚手架进程，并执行它，然后就执行了 init 程序，init 执行了用户态的一些初始化工作，之后就结束了。

## 内核中各个模块的解释

TODO
