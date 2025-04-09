#pragma once
#include <cstdint>

#include "file.h"

namespace loader {
constexpr uint32_t MAXARGV{32};
constexpr uint32_t USERSTACK{1};

auto exec(struct file::inode *ip, char *path, char **argv) -> int;
}  // namespace loader
