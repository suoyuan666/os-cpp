#include <cstdint>

#ifndef ARCH_RISCV
#include "arch/riscv.h"
#define ARCH_RISCV
#endif
#include <fmt>

#include "bio.h"
#include "console.h"
#include "file.h"
#include "fs.h"
#include "plic.h"
#include "proc.h"
#include "trap.h"
#include "virtio_disk.h"
#include "vm.h"

__attribute__((aligned(16))) volatile unsigned char stack0[4096 * proc::NCPU];
volatile static bool started{false};

auto main() -> void;

auto timerinit() -> void {
  w_mie(r_mie() | MIE_STIE);
  w_menvcfg(r_menvcfg() | (1UL << 63U));
  w_mcounteren(r_mcounteren() | 2U);
  w_stimecmp(r_time() + 1000000U);
}

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

  timerinit();

  auto id = r_mhartid();
  w_tp(id);

  asm volatile("mret");
}

auto main() -> void {
  if (proc::cpuid() == 0) {
    console::init();
    fmt::print("\n");
    fmt::print_log(fmt::log_level::INFO, "console init successful\n");
    vm::kinit();
    vm::init();
    vm::inithart();
    fmt::print_log(fmt::log_level::INFO, "vm init successful\n");
    proc::init();
    fmt::print_log(fmt::log_level::INFO, "proc init successful\n");
    trap::inithart();
    plic::init();
    plic::inithart();
    bio::init();
    fmt::print_log(fmt::log_level::INFO, "file system init successful\n");
    virtio_disk::init();
    fmt::print_log(fmt::log_level::INFO, "disk init successful\n");
    proc::user_init();
    __sync_synchronize();
    started = true;
  } else {
    while (started == false);
    __sync_synchronize();
    fmt::print("hart {} starting\n", proc::cpuid());
    vm::inithart();
    trap::inithart();
    plic::inithart();
  }
  proc::scheduler();
}
