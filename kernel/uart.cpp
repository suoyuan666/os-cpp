#include <cstdint>
#include <optional>

#include "console.h"
#include "lock.h"
#include "proc.h"

namespace uart {
constexpr uint64_t RHR{0};
constexpr uint64_t THR{0};
constexpr uint64_t IER{1};
constexpr uint64_t IER_RX_ENABLE{(1ULL << 0U)};
constexpr uint64_t IER_TX_ENABLE{(1ULL << 1U)};
constexpr uint64_t FCR{2};
constexpr uint64_t FCR_FIFO_ENABLE{1U << 0U};
constexpr uint64_t FCR_FIFO_CLEAR{3U << 1U};
constexpr uint64_t ISR{2};
constexpr uint64_t LCR{3};
constexpr uint64_t LCR_EIGHT_BITS{3U << 0U};
constexpr uint64_t LCR_BAUD_LATCH{1U << 7U};
constexpr uint64_t LSR{5};
// constexpr uint64_t LSR_RX_READY {1<<0};
constexpr uint64_t LSR_TX_IDLE{1U << 5U};

class lock::spinlock uart_tx_lock{"uart"};

constexpr uint32_t UART_TX_BUF_SIZE{32};
char uart_tx_buf[UART_TX_BUF_SIZE];
uint64_t uart_tx_w;
uint64_t uart_tx_r;

constexpr uint64_t UART_BASE{0x10000000ULL};

static inline auto reg(const uint64_t offset) -> char* {
  return (char*)(UART_BASE + offset);
};

static inline auto read_reg(const uint64_t offset) -> char {
  return *reg(offset);
};
static inline auto write_reg(const uint64_t offset, const uint64_t value)
    -> void {
  *reg(offset) = value;
};

auto init() -> void {
  write_reg(IER, 0x00);
  write_reg(LCR, LCR_BAUD_LATCH);
  write_reg(0, 0x03);
  write_reg(1, 0x00);
  write_reg(LCR, LCR_EIGHT_BITS);
  write_reg(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);
  write_reg(IER, IER_RX_ENABLE | IER_TX_ENABLE);
}

auto start() -> void {
  while (true) {
    if (uart_tx_w == uart_tx_r) {
      read_reg(ISR);
      return;
    }

    if ((read_reg(LSR) & LSR_TX_IDLE) == 0) {
      return;
    }

    int c = uart_tx_buf[uart_tx_r % UART_TX_BUF_SIZE];
    uart_tx_r += 1;

    proc::wakeup(&uart_tx_r);

    write_reg(THR, c);
  }
}

auto putc(char c) -> void {
  uart_tx_lock.acquire();

  while (uart_tx_w == uart_tx_r + UART_TX_BUF_SIZE) {
    proc::sleep(&uart_tx_r, uart_tx_lock);
  }

  uart_tx_buf[uart_tx_w % UART_TX_BUF_SIZE] = c;
  ++uart_tx_w;
  start();
  uart_tx_lock.release();
}

auto kputc(char c) -> void {
  lock::push_off();

  while ((read_reg(LSR) & LSR_TX_IDLE) == 0);
  write_reg(THR, c);

  lock::pop_off();
}

auto getc() -> std::optional<char> {
  if (read_reg(LSR) & 0x01ULL) {
    auto rs = read_reg(RHR);
    return {rs};
  }
  return {};
}

auto intr() -> void {
  while (true) {
    auto opt_c = getc();
    if (!opt_c.has_value()) {
      break;
    }
    console::intr(opt_c.value());
  }

  uart_tx_lock.acquire();
  start();
  uart_tx_lock.release();
}
}  // namespace uart
