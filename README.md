尝试使用 C++ 20 写一个简单的操作系统，目标平台是 RISC-V 64

目前完全依赖于 Clang/LLVM 构建

该项目计划包含一个内核、libc、libc++、coreutils

更多关于项目的讲解在 [wiki](https://github.com/suoyuan666/os-cpp/wiki)

虽然目前 wiki 界面没太多内容，因为该项目进度就在这里，我准备在 wiki 界面讲解一下这个项目，写哪部分了就讲哪部分，从而能帮助到其他人（前提是这个项目不烂尾，并且还算有点质量😶‍🌫️）

## 如何使用

首先，确保本机安装了 Clang/LLVM 编译器。除此之外还要有 qemu 针对 RISC-V 架构的模拟器，这个软件包一般可能叫 qemu-system-misc, qemu-emulators-full, qemu-system-riscv 之类的

部分发行版安装了 clang 和 llvm 之后可能没安装 lld 链接器，需要单独安装

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

设置了之后直接运行 `gdb` 即可

### lldb

对于 lldb 的话，项目根目录下也存在一个 .lldbinit 文件用于读取

lldb 同样也是默认禁止这些非信任文件夹的 lldbinit 的读取，但是貌似不支持对特定目录的信任，所以我现在暂时运行 `lldb --local-lldbinit` 来读取项目根目录的 .lldbinit

更多的可以看 [Kernel Debugging](https://wiki.osdev.org/Kernel_Debugging#Using_Debuggers_with_VMs)

## 参考项目

[mit-pdos/xv6-riscv](https://github.com/mit-pdos/xv6-riscv)

[musl libc](https://musl.libc.org/)
