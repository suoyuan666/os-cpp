#pragma once
#include <optional>

namespace uart {
auto init() -> void;
auto putc(char c) -> void;
auto kputc(char c) -> void;
auto intr() -> void;
auto getc() -> std::optional<char>;
}  // namespace uart
