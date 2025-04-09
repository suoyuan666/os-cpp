#pragma once
#include <cstdint>

namespace syscall {
constexpr uint32_t SYS_fork{0};
constexpr uint32_t SYS_exit{1};
constexpr uint32_t SYS_wait{2};
constexpr uint32_t SYS_pipe{3};
constexpr uint32_t SYS_read{4};
constexpr uint32_t SYS_kill{5};
constexpr uint32_t SYS_exec{6};
constexpr uint32_t SYS_fstat{7};
constexpr uint32_t SYS_chdir{8};
constexpr uint32_t SYS_dup{9};
constexpr uint32_t SYS_getpid{10};
constexpr uint32_t SYS_sbrk{11};
constexpr uint32_t SYS_sleep{12};
constexpr uint32_t SYS_uptime{13};
constexpr uint32_t SYS_open{14};
constexpr uint32_t SYS_write{15};
constexpr uint32_t SYS_mknod{16};
constexpr uint32_t SYS_unlink{17};
constexpr uint32_t SYS_link{18};
constexpr uint32_t SYS_mkdir{19};
constexpr uint32_t SYS_close{20};
constexpr uint32_t SYS_setuid{21};
constexpr uint32_t SYS_setgid{22};

auto fetch_addr(uint64_t addr, uint64_t *ip) -> bool;
auto fetch_str(uint64_t addr, char *buf, uint32_t len) -> bool;
auto get_argu(uint32_t index) -> uint64_t;
auto syscall() -> void;
}  // namespace syscall
