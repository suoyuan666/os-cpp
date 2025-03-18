#include "fmt"

#include "console"
#include "lock"

namespace fmt {
auto puts(const char *str) -> void {
  if (str != nullptr && *str != '\0') {
    for (; *str != '\0'; ++str) {
      console::putc(*str);
    }
  }
}

auto print_format(const char *fmt) -> void { puts(fmt); }

auto print(const char *str) -> void {
  auto locked = pr.locked;
  if (locked) {
    pr.lock.acquire();
  }
  puts(str);
  if (locked) {
    pr.lock.release();
  }
}

auto panic(const char *str) -> void {
  puts("panic: ");
  puts(str);
  puts("\n");
  while (true) {
  };
}

auto print_log(log_level level, const char *msg) -> void {
  switch (level) {
    case log_level::INFO:
      puts("[INFO]: ");
      break;
    case log_level::DEBUG:
      puts("[DEBUG]: ");
      break;
    case log_level::WARNNING:
      puts("[WARNNING]: ");
      break;
    case log_level::ERROR:
      puts("[ERROR]: ");
      break;
  }
  puts(msg);
};

}  // namespace fmt
