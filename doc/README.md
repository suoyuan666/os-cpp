# os-cpp
## 简介

os-cpp 是一个使用 C++ 20 标准编写，目标架构为 RISC-V 64 的操作系统。

该项目不仅局限于一个操作系统内核，还包含一个简单的用户态必要工具，如 libc、coreutils

## 项目结构

```txt
.
├── CMakeLists.txt
├── kernel/
├── LICENSE
├── Makefile
├── mkfs
│   ├── CMakeLists.txt
│   └── mkfs.cpp
├── README.md
├── ulibc
│   ├── arch/
│   ├── CMakeLists.txt
│   ├── crt/
│   ├── include/
│   ├── obj/
│   └── src/
├── ulibc++
│   ├── CMakeLists.txt
│   ├── include/
│   └── src/
└── utils/
```

- kernel
    - 存放内核部分的代码
- mkfs
    - 用于生成系统运行所需的硬盘镜像
- ulibc++
    - C++ 标准库部分，目前的实现没有什么进展
- ulibc
    - include 是 C 标准库中用于让用户程序 include 的头文件
    - src 是 C 标准库的具体实现
    - crt 是 C 程序运行时的实现
    - arch 是 C 标准库中和 CPU 架构相关的代码
    - obj 存放了 C 标准库实现时使用的一些数据结构等代码
- utils
    - 存放了用户程序的实现，包括 init 程序

## 各个部分的介绍

- [内核部分](./kernel/README.md)
- [libc 部分](./libc/README.md)

# FAQ

- 为什么选择了 RISC-V
    - 受到 xv6-riscv 的影响，相比于 x86-64 等架构来说，我对 RISC-V 的一些机制了解的更多
- 为什么选择了 LLVM 作为项目唯一指定的编译器
    - 得益于 LLVM 的模块化架构，本机安装的 Clang 可以直接指定目标运行的 CPU 架构，不需要像 GCC 那样需要额外安装一份 riscv64-unknown-linux-gnu-gcc 之类的 GCC 
