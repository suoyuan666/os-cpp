#include <cstdint>

#include "arch/riscv"

extern "C" auto kernelvec() -> void;

namespace trap {
auto inithart() -> void { w_stvec((uint64_t)kernelvec); }
}  // namespace trap
