#define UART0_BASE 0x10000000

__attribute__((aligned(16))) char stack0[4096];

extern "C" void main() {
  char str[24] = "test for start kernel\n";
  for (const auto& val : str) {
    *(volatile char*)UART0_BASE = val;
  }
  while (true) {
  };
}