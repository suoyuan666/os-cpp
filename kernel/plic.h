#pragma once
namespace plic {
auto init() -> void;
auto inithart() -> void;
auto plic_claim() -> int;
auto plic_complete(int irq) -> void;
} // namespace plic
