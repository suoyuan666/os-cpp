#include "console.h"

#include <cstdint>

#include "file.h"
#include "lock.h"
#include "proc.h"
#include "uart.h"

namespace console {
constexpr uint32_t INPUT_BUF_SIZE{128};
constexpr uint32_t BACKSPACE{0x100};

constexpr auto C = [](int x) -> int { return ((x) - '@'); };

struct {
  class lock::spinlock lock{};

  // input
  char buf[INPUT_BUF_SIZE]{};
  uint32_t r{};  // Read index
  uint32_t w{};  // Write index
  uint32_t e{};  // Edit index
} cons{};

auto putc(int c) -> void {
  if (c == BACKSPACE) {
    uart::kputc('\b');
    uart::kputc(' ');
    uart::kputc('\b');
  } else {
    uart::kputc(c);
  }
}

auto write(bool user_src, uint64_t src, int n) -> int {
  auto i{0};
  for (; i < n; ++i) {
    char c = 0;
    if (proc::either_copyin(&c, user_src, src + i, 1) != 1) {
      break;
    }
    uart::putc(c);
  }

  return i;
}

auto read(bool user_dst, uint64_t dst, int n) -> int {
  auto target = n;
  cons.lock.acquire();
  while (n > 0) {
    // wait until interrupt handler has put some
    // input into cons.buffer.
    while (cons.r == cons.w) {
      if (proc::get_killed(proc::curr_proc())) {
        cons.lock.release();
        return -1;
      }
      proc::sleep(&cons.r, cons.lock);
    }

    auto c = cons.buf[cons.r++ % INPUT_BUF_SIZE];

    if (c == C('D')) {  // end-of-file
      if (n < target) {
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        cons.r--;
      }
      break;
    }

    // copy the input byte to the user-space buffer.
    auto cbuf = c;
    if (proc::either_copyout(user_dst, dst, &cbuf, 1) == -1) {
      break;
    }

    dst++;
    --n;

    if (c == '\n') {
      // a whole line has arrived, return to
      // the user-level read().
      break;
    }
  }
  cons.lock.release();

  return target - n;
}

void intr(int c) {
  cons.lock.acquire();

  switch (c) {
    case C('U'):  // Kill line.
      while (cons.e != cons.w &&
             cons.buf[(cons.e - 1) % INPUT_BUF_SIZE] != '\n') {
        cons.e--;
        putc(BACKSPACE);
      }
      break;
    case C('H'):  // Backspace
    case '\x7f':  // Delete key
      if (cons.e != cons.w) {
        cons.e--;
        putc(BACKSPACE);
      }
      break;
    default:
      if (c != 0 && cons.e - cons.r < INPUT_BUF_SIZE) {
        c = (c == '\r') ? '\n' : c;

        // echo back to the user.
        putc(c);

        // store for consumption by consoleread().
        cons.buf[cons.e++ % INPUT_BUF_SIZE] = c;

        if (c == '\n' || c == C('D') || cons.e - cons.r == INPUT_BUF_SIZE) {
          // wake up consoleread() if a whole line (or end-of-file)
          // has arrived.
          cons.w = cons.e;
          proc::wakeup(&cons.r);
        }
      }
      break;
  }

  cons.lock.release();
}

auto init() -> void {
  uart::init();

  // connect read and write system calls
  // to consoleread and consolewrite.
  file::devsw[file::CONSOLE].read = read;
  file::devsw[file::CONSOLE].write = write;
}
}  // namespace console
