#include "type_def"

constexpr uint64_t UART_BASE {0x10000000ULL};

constexpr auto reg = [](const uint64_t offset) -> char* {
  return (char*)(UART_BASE + offset);
};

constexpr auto read_reg = [](const uint64_t offset) -> char {
  return *reg(offset);
};
constexpr auto write_reg = [](const uint64_t offset, const uint64_t value) -> void {
  *reg(offset) = value;
};

// constexpr uint64_t RHR {0};  
constexpr uint64_t THR {0};
constexpr uint64_t IER {1};
constexpr uint64_t IER_RX_ENABLE {(1ULL<<0U)};
constexpr uint64_t IER_TX_ENABLE {(1ULL<<1U)};
constexpr uint64_t FCR {2}; 
constexpr uint64_t FCR_FIFO_ENABLE {1<<0};
constexpr uint64_t FCR_FIFO_CLEAR {3<<1};
// constexpr uint64_t ISR {2};
constexpr uint64_t LCR {3};
constexpr uint64_t LCR_EIGHT_BITS {3<<0};
constexpr uint64_t LCR_BAUD_LATCH {1<<7}; 
constexpr uint64_t LSR {5};
// constexpr uint64_t LSR_RX_READY {1<<0};
constexpr uint64_t LSR_TX_IDLE {1<<5};


auto uartinit() -> void {
  write_reg(IER, 0x00);
  write_reg(LCR, LCR_BAUD_LATCH);
  write_reg(0, 0x03);
  write_reg(1, 0x00);
  write_reg(LCR, LCR_EIGHT_BITS);
  write_reg(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);
  write_reg(IER, IER_RX_ENABLE | IER_TX_ENABLE);
}

auto uart_putc(char c) -> void {
  while ((read_reg(LSR) & LSR_TX_IDLE) == 0) {
  };
  write_reg(THR, c); 
}
