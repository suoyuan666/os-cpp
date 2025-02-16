#pragma once

#include <cstddef>
namespace std {
class ios_base {
 public:
  enum class iostate { goodbit = 0, badbit = 1, eofbit = 2, failbit = 4 };
  enum seekdir { beg = 0, cur = 1, end = 2 };

  ios_base() = default;
  virtual ~ios_base() = default;

  [[nodiscard]] auto state() const -> iostate { return state_; }

  void setstate(iostate state) { state_ = state; }

  [[nodiscard]] auto good() const -> bool { return state_ == iostate::goodbit; }
  [[nodiscard]] auto fail() const -> bool {
    return static_cast<int>(state_) & static_cast<int>(iostate::failbit);
  }
  [[nodiscard]] auto eof() const -> bool {
    return static_cast<int>(state_) & static_cast<int>(iostate::eofbit);
  }
  [[nodiscard]] auto bad() const -> bool {
    return static_cast<int>(state_) & static_cast<int>(iostate::badbit);
  }

 protected:
  iostate state_{};
};

using streamoff = long long;
using streamsize = size_t;
}  // namespace std
