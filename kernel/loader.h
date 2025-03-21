#pragma once
#include <cstdint>

namespace loader {
constexpr uint32_t MAXARGV{32};
constexpr uint32_t USERSTACK{1};

auto exec(char *path, char **argv) -> int;
}  // namespace loader
