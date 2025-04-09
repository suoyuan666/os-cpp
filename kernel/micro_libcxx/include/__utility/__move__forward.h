#pragma once

#include <type_traits>
namespace std {
template <typename T>
using remove_reference_t = typename remove_reference<T>::type;

template <typename T>
constexpr typename std::remove_reference<T>::type&& move(T&& t) noexcept {
  return static_cast<typename std::remove_reference<T>::type&&>(t);
}

template <typename T>
constexpr T&& forward(typename std::remove_reference<T>::type& t) noexcept {
  return static_cast<T&&>(t);
}

template <typename T>
constexpr T&& forward(typename std::remove_reference<T>::type&& t) noexcept {
  static_assert(!std::is_lvalue_reference<T>::value,
                "forward cannot be used to convert an rvalue to an lvalue");
  return static_cast<T&&>(t);
}
}  // namespace std
