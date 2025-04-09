#pragma once
#include <cstddef>

namespace std {
template <typename T>
class initializer_list {
 private:
  const T* _first;
  size_t _size;

  constexpr initializer_list(const T* first, size_t size) noexcept
      : _first(first), _size(size) {}

 public:
  using value_type = T;
  using reference = const T&;
  using const_reference = const T&;
  using size_type = size_t;
  using iterator = const T*;
  using const_iterator = const T*;

  constexpr initializer_list() noexcept : _first(nullptr), _size(0) {}

  [[nodiscard]] constexpr auto size() const noexcept -> size_t { return _size; }

  constexpr auto begin() const noexcept -> const T* { return _first; }
  constexpr auto end() const noexcept -> const T* { return _first + _size; }
};

template <typename T>
constexpr auto begin(initializer_list<T> il) noexcept -> const T* {
  return il.begin();
}

template <typename T>
constexpr auto end(initializer_list<T> il) noexcept -> const T* {
  return il.end();
}
}  // namespace std

