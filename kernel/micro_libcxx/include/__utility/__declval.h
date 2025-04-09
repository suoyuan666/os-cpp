#pragma once

#include <type_traits>

namespace std {
template <typename T>
using add_rvalue_reference_t = typename add_rvalue_reference<T>::type;

template <typename T>
auto declval() noexcept -> add_rvalue_reference_t<T> {
  static_assert(sizeof(T) != 0, "incomplete type is not allowed");
  static_assert(false,
                "std::declval() must not be used in an evaluated context");
}

}  // namespace std
