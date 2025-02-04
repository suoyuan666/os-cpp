#include <cstdint>

#include "proc"
#include "trap"
#include "uart"
#include "vm"

__attribute__((aligned(16))) char stack0[4096];

auto main() -> void;

extern "C" void start() {
  auto mstatus = r_mstatus();
  mstatus &= ~MSTATUS_MPP_MASK;
  mstatus |= MSTATUS_MPP_S;

  w_mstatus(mstatus);

  w_mepc((uint64_t)main);

  w_satp(0);

  w_medeleg(0xffff);
  w_mideleg(0xffff);
  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

  w_pmpaddr0(0x3fffffffffffffULL);
  w_pmpcfg0(0xf);

  asm volatile("mret");
}

auto main() -> void {
  uart::init();
  vm::init();
  vm::inithart();
  proc::init();
  trap::inithart();
}
