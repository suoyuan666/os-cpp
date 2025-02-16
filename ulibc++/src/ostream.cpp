#include <cstddef>
#include <ostream>
#include <streambuf>

#include "kernel/scall"

namespace std {

template <class CharT, class Traits = std::char_traits<CharT>>
class stdoutbuf : public basic_streambuf<CharT, Traits> {
 public:
  stdoutbuf() = default;
  ~stdoutbuf()  = default;

 protected:
  auto overflow(Traits::int_type ch) -> Traits::int_type override {
    if (ch != Traits::eof()) {
      basic_streambuf<char>::sputc(Traits::to_char_type(ch));
    }

    if (this->pptr() > this->pbase()) {
      size_t len = this->pptr() - this->pbase();
      size_t bytes_written = write(1, this->pbase(), len);
      if (bytes_written < 0) {
        return Traits::eof();
      }
      this->pbump(-static_cast<int>(len));
    }

    return ch;
  }
};

stdoutbuf<char> _stdout_buf{};
ostream cout{&_stdout_buf};
}  // namespace std
