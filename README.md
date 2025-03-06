尝试使用 C++ 20 写一个简单的操作系统，目标平台是 RISC-V 64

目前规划包括

- [ ] kernel
- [ ] coreutils
  - ls
  - cat
  - ...
- [ ] libcxx
  - 实现一个简单的，目前也没想要去完整实现某个标准的 libc++，实现一些基本的标准库函数吧
  - 实现一个简单标准的 libcxx 比我想象中要更花时间，先看看实现一个简单的 libc 让用户程序能用起来先（
- [ ] libc
  - 和 libcxx 目标一致

目前完全依赖于 Clang/LLVM 构建

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

## 参考项目

[mit-pdos/xv6-riscv](https://github.com/mit-pdos/xv6-riscv)

[musl libc](https://musl.libc.org/)
