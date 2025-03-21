#pragma once

#include <cstdint>
namespace proc {
struct cpu;
}  // namespace proc

namespace lock {
class spinlock {
  bool locked{false};

  struct proc::cpu *cpu{nullptr};

 public:
  spinlock() = default;

  auto acquire() -> void;
  auto release() -> void;

  auto holding() -> bool;
};

auto push_off() -> void;
auto pop_off() -> void;

class sleeplock {
  bool locked{false};
  class spinlock lk{};

  uint32_t pid{0};

 public:
  sleeplock() = default;

  auto acquire() -> void;
  auto release() -> void;

  auto holding() -> bool;
};
}  // namespace lock
