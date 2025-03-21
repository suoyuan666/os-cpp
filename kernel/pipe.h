#pragma once
#include <cstdint>

#include "lock.h"

namespace file {
constexpr uint32_t PIPESIZE{512};

struct pipe {
  class lock::spinlock lock{};
  char data[PIPESIZE];
  uint32_t nread;   // number of bytes read
  uint32_t nwrite;  // number of bytes written
  bool readopen;    // read fd is still open
  bool writeopen;   // write fd is still open
};

auto piperead(struct pipe *pi, uint64_t addr, int n) -> uint32_t;
auto pipewrite(struct pipe *pi, uint64_t addr, int n) -> uint32_t;
auto pipeclose(struct pipe *pi, bool writable) -> void;
auto pipealloc(struct file **f0, struct file **f1) -> int;
}  // namespace file
