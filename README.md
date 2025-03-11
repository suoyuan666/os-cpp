尝试使用 C++ 20 写一个简单的操作系统，目标平台是 RISC-V 64

目前完全依赖于 Clang/LLVM 构建

该项目计划包含一个内核、libc、libc++、coreutils

如果想看更多关于该项目的介绍请看 doc 文件夹

## 如何使用

由于该项目显示指定了编译器是 Clang，所以需要安装它。一般来说，安装 clang 的时候，llvm 会被作为依赖一起安装，所以只需要手动指定安装 clang 和 qemu 即可

对于 qemu 来说，我们需要安装目标模拟架构为 RISC-V 的 qemu，而非单纯使用包管理器安装 qemu。

- 对于 Debian 来说，应该安装的软件包名是 [qemu-system-misc](https://packages.debian.org/bookworm/qemu-system-misc)
- 对于 Fedora 来说，应该安装的软件包名是 [qemu-system-riscv](https://packages.fedoraproject.org/pkgs/qemu/qemu-system-riscv/index.html)
- 对于 Arch Linux 来说，应该安装的软件包名是 [qemu-system-riscv](https://archlinux.org/packages/extra/x86_64/qemu-system-riscv/)
- 对于 Gentoo Linux 来说，应该给 app-emulation/qemu 的 USE 变量添加一个 `qemu_softmmu_targets_riscv32 qemu_softmmu_targets_riscv64` 重新编译即可

Debian 的 qemu-system-misc 是把所有支持模拟的架构都装上了，这不太好，但是对于 stable 分支来说，貌似也只能这样，除非你手动编译。Debian 的 tesing 分支有 [qemu-system-riscv](https://packages.debian.org/trixie/qemu-system-riscv)

当然，还需要安装 cmake 和 make

```bash
$ cmake -S . -B build
$ cmake --build build
$ make fs
$ make qemu
```

## 调试

### gdb

项目根目录存在 .gdbinit 文件，gdb 在该目录运行时会自动读取。

上面这句话的前提是该目录是被信任的，被信任的目录会被手动写入到一个配置文件中，或者可以直接在配置文件中写明信任任何目录

直接在项目根目录下运行 `gdb` 就会给出读取本地 .gdbinit 的提示，因为默认情况下会禁止本地 .gdbinit 的读取，它会提示需要有相关设置才能读取

设置了之后直接运行 `gdb` 即可，或者如果你所运行的系统可以安装类似 riscv64-unknown-linux-gnu-gdb 的话也可以运行这位

### lldb

对于 lldb 的话，项目根目录下也存在一个 .lldbinit 文件用于读取

lldb 同样也是默认禁止这些非信任文件夹的 lldbinit 的读取，但是貌似不支持对特定目录的信任，所以我现在暂时运行 `lldb --local-lldbinit` 来读取项目根目录的 .lldbinit

更多的可以看 [Kernel Debugging](https://wiki.osdev.org/Kernel_Debugging#Using_Debuggers_with_VMs)

## 参考项目

- [mit-pdos/xv6-riscv](https://github.com/mit-pdos/xv6-riscv)

- [musl libc](https://musl.libc.org/)
