#pragma once
#include <cstdint>

namespace syscall {
constexpr uint32_t SYS_fork {1};
constexpr uint32_t SYS_exit {2};
constexpr uint32_t SYS_wait {3};
constexpr uint32_t SYS_pipe {4};
constexpr uint32_t SYS_read {5};
constexpr uint32_t SYS_kill {6};
constexpr uint32_t SYS_exec {7};
constexpr uint32_t SYS_fstat {8};
constexpr uint32_t SYS_chdir {9};
constexpr uint32_t SYS_dup {10};
constexpr uint32_t SYS_getpid {11};
constexpr uint32_t SYS_sbrk {12};
constexpr uint32_t SYS_sleep {13};
constexpr uint32_t SYS_uptime {14};
constexpr uint32_t SYS_open {15};
constexpr uint32_t SYS_write {16};
constexpr uint32_t SYS_mknod {17};
constexpr uint32_t SYS_unlink {18};
constexpr uint32_t SYS_link {19};
constexpr uint32_t SYS_mkdir {20};
constexpr uint32_t SYS_close {21};

auto fetch_addr(uint64_t addr, uint64_t *ip) -> bool;
auto fetch_str(uint64_t addr, char *buf, uint32_t len) -> bool;
auto get_argu(uint32_t index) -> uint64_t;
auto syscall() -> void;
}  // namespace syscall
