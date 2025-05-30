#pragma once
#include <cstdint>

namespace console {
auto init() -> void;
void intr(int c);
auto read(bool user_dst, uint64_t dst, int n) -> int;
auto write(bool user_src, uint64_t src, int n) -> int;
auto putc(int c) -> void;
} // namespace console
