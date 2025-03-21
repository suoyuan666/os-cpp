// -*- C++ -*-

#pragma once
#include <cstdint>

// for RISC-V

__attribute__((always_inline)) static inline auto r_mhartid() -> uint64_t {
  uint64_t x = 0;
  asm volatile("csrr %0, mhartid" : "=r"(x));
  return x;
}

// Machine

constexpr uint64_t MSTATUS_MIE{1ULL < 3U};
constexpr uint64_t MSTATUS_MPP_MASK{3ULL << 11U};
constexpr uint64_t MSTATUS_MPP_M{3ULL << 11U};
constexpr uint64_t MSTATUS_MPP_S{1ULL << 11U};
constexpr uint64_t MSTATUS_MPP_U{0ULL << 11U};

constexpr uint64_t MIE_STIE{1U << 5U};
__attribute__((always_inline)) static inline auto r_mie() -> uint64_t {
  uint64_t x = 0;
  asm volatile("csrr %0, mie" : "=r"(x));
  return x;
}
__attribute__((always_inline)) static inline auto w_mie(uint64_t x) -> void {
  asm volatile("csrw mie, %0" : : "r"(x));
}

__attribute__((always_inline)) static inline auto r_menvcfg() -> uint64_t {
  uint64_t x = 0;
  asm volatile("csrr %0, 0x30a" : "=r"(x));
  return x;
}
__attribute__((always_inline)) static inline auto w_menvcfg(uint64_t x)
    -> void {
  asm volatile("csrw 0x30a, %0" : : "r"(x));
}

__attribute__((always_inline)) static inline auto w_mcounteren(uint64_t x)
    -> void {
  asm volatile("csrw mcounteren, %0" : : "r"(x));
}
__attribute__((always_inline)) static inline auto r_mcounteren() -> uint64_t {
  uint64_t x = 0;
  asm volatile("csrr %0, mcounteren" : "=r"(x));
  return x;
}

__attribute__((always_inline)) static inline auto r_mstatus() -> uint64_t {
  uint64_t rs{};
  asm volatile("csrr %0, mstatus" : "=r"(rs));
  return rs;
};
__attribute__((always_inline)) static inline auto w_mstatus(uint64_t value)
    -> void {
  asm volatile("csrw mstatus, %0" : : "r"(value));
};

__attribute__((always_inline)) static inline auto w_mepc(uint64_t addr)
    -> void {
  asm volatile("csrw mepc, %0" : : "r"(addr));
};

__attribute__((always_inline)) static inline auto w_satp(uint64_t value)
    -> void {
  asm volatile("csrw satp, %0" : : "r"(value));
};

__attribute__((always_inline)) static inline auto r_satp() -> uint64_t {
  uint64_t x{};
  asm volatile("csrr %0, satp" : "=r"(x));
  return x;
}

__attribute__((always_inline)) static inline auto w_medeleg(uint64_t value)
    -> void {
  asm volatile("csrw medeleg, %0" : : "r"(value));
};

__attribute__((always_inline)) static inline auto w_mideleg(uint64_t value)
    -> void {
  asm volatile("csrw mideleg, %0" : : "r"(value));
};

__attribute__((always_inline)) static inline auto w_pmpaddr0(uint64_t value)
    -> void {
  asm volatile("csrw pmpaddr0, %0" : : "r"(value));
};
__attribute__((always_inline)) static inline auto w_pmpcfg0(uint64_t value)
    -> void {
  asm volatile("csrw pmpcfg0, %0" : : "r"(value));
};

__attribute__((always_inline)) static inline auto r_time() -> uint64_t {
  uint64_t x = 0;
  asm volatile("csrr %0, time" : "=r"(x));
  return x;
}

// Supervisor

constexpr uint64_t SIE_SEIE{1U << 9U};  // extenal
constexpr uint64_t SIE_STIE{1U << 5U};  // time
constexpr uint64_t SIE_SSIE{1U << 1U};  // software

// clang-format off
constexpr uint32_t SSTATUS_SPP{1U << 8U};  // Previous mode, 1=Supervisor, 0=User
constexpr uint32_t SSTATUS_SPIE{1U << 5U};  // Supervisor Previous Interrupt Enable
constexpr uint32_t SSTATUS_UPIE{1U << 4U};  // User Previous Interrupt Enable
constexpr uint32_t SSTATUS_SIE{1U << 1U};   // Supervisor Interrupt Enable
constexpr uint32_t SSTATUS_UIE{1U << 0U};   // User Interrupt Enable
// clang-format on

__attribute__((always_inline)) static inline auto w_sie(uint64_t value)
    -> void {
  asm volatile("csrw sie, %0" : : "r"(value));
};
__attribute__((always_inline)) static inline auto r_sie() -> uint64_t {
  uint64_t rs{};
  asm volatile("csrr %0, sie" : "=r"(rs));
  return rs;
};

__attribute__((always_inline)) static inline auto sfence_vma() -> void {
  asm volatile("sfence.vma zero, zero");
};

__attribute__((always_inline)) static inline auto w_stvec(uint64_t value)
    -> void {
  asm volatile("csrw stvec, %0" : : "r"(value));
};

__attribute__((always_inline)) static inline auto w_sstatus(uint64_t value)
    -> void {
  asm volatile("csrw sstatus, %0" : : "r"(value));
};
__attribute__((always_inline)) static inline auto r_sstatus() -> uint64_t {
  uint64_t rs{};
  asm volatile("csrr %0, sstatus" : "=r"(rs));
  return rs;
};

__attribute__((always_inline)) static inline auto r_scause() -> uint64_t {
  uint64_t rs{};
  asm volatile("csrr %0, scause" : "=r"(rs));
  return rs;
};

__attribute__((always_inline)) static inline auto w_sepc(uint64_t x) -> void {
  asm volatile("csrw sepc, %0" : : "r"(x));
}
__attribute__((always_inline)) static inline auto r_sepc() -> uint64_t {
  uint64_t rs{};
  asm volatile("csrr %0, sepc" : "=r"(rs));
  return rs;
}

__attribute__((always_inline)) static inline auto r_stval() -> uint64_t {
  uint64_t rs{};
  asm volatile("csrr %0, stval" : "=r"(rs));
  return rs;
};

__attribute__((always_inline)) static inline auto r_stimecmp() -> uint64_t {
  uint64_t x = 0;
  asm volatile("csrr %0, 0x14d" : "=r"(x));
  return x;
}

__attribute__((always_inline)) static inline auto w_stimecmp(uint64_t x)
    -> void {
  asm volatile("csrw 0x14d, %0" : : "r"(x));
}

__attribute__((always_inline)) static inline auto w_tp(uint64_t x) -> void {
  asm volatile("mv tp, %0" : : "r"(x));
}
__attribute__((always_inline)) static inline auto r_tp() -> uint64_t {
  uint64_t x = 0;
  asm volatile("mv %0, tp" : "=r"(x));
  return x;
}

// for page table
constexpr uint64_t PTE_V{1ULL << 0U};
constexpr uint64_t PTE_R{1ULL << 1U};
constexpr uint64_t PTE_W{1ULL << 2U};
constexpr uint64_t PTE_X{1ULL << 3U};
constexpr uint64_t PTE_U{1ULL << 4U};

constexpr uint32_t PGSIZE{4096U};

__attribute__((always_inline)) static inline auto PG_ROUND_UP(uint64_t addr)
    -> uint64_t {
  return (addr + PGSIZE - 1) & ~(PGSIZE - 1);
};
__attribute__((always_inline)) static inline auto PG_ROUND_DOWN(uint64_t addr)
    -> uint64_t {
  return (addr) & ~(PGSIZE - 1);
};

__attribute__((always_inline)) static inline auto PTE2PA(uint64_t pte)
    -> uint64_t {
  return (pte >> 10U) << 12U;
};
__attribute__((always_inline)) static inline auto PA2PTE(uint64_t pa)
    -> uint64_t {
  return (pa >> 12U) << 10U;
};

__attribute__((always_inline)) static inline auto PTE_FLAGS(uint64_t pte)
    -> uint64_t {
  return pte & 1023U;  // (1023)_10 == (11,1111,1111)_2
};

constexpr uint32_t PX_MASK{0x1FF};
constexpr uint32_t PG_SHIFT{12};
__attribute__((always_inline)) static inline auto PX_SHIFT(uint32_t level)
    -> uint64_t {
  return PG_SHIFT + 9 * level;
};
__attribute__((always_inline)) static inline auto PX(uint32_t level,
                                                     uint64_t pte) -> uint64_t {
  return (pte >> PX_SHIFT(level)) & PX_MASK;
};

// Sv39 就是如此
constexpr uint64_t VA_MAX{1ULL << (9U + 9 + 9 + 12 - 1)};
constexpr uint64_t SATP_SV39{8UL << 60U};

__attribute__((always_inline)) static inline auto MAKE_SATP(uint64_t* pagetable)
    -> uint64_t {
  return SATP_SV39 | ((uint64_t)pagetable >> 12U);
};

__attribute__((always_inline)) static inline auto intr_on() -> void {
  w_sstatus(r_sstatus() | SSTATUS_SIE);
}

__attribute__((always_inline)) static inline auto intr_off() -> void {
  w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}

__attribute__((always_inline)) static inline auto intr_get() -> uint64_t {
  uint64_t x = r_sstatus();
  return (x & SSTATUS_SIE) != 0;
}
