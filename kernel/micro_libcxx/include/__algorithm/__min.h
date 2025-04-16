#pragma once
#include <utility>

namespace std {

template <class T>
constexpr auto min(const T& a, const T& b) -> const T& {
  // NOLINTBEGIN
  return (b < a) ? b : a;
  // NOLINTEND
}

template <class T, class Compare>
constexpr auto min(const T& a, const T& b, Compare comp) -> const T& {
  // NOLINTBEGIN
  return comp(b, a) ? b : a;
  // NOLINTEND
}

template <class T>
constexpr auto min(std::initializer_list<T> ilist) -> T {
  return *min(ilist.begin(), ilist.end());
}

template <class T, class Compare>
constexpr auto min(std::initializer_list<T> ilist, Compare comp) -> T {
  return *min(ilist.begin(), ilist.end(), std::move(comp));
}

template <class T, class... Args>
constexpr auto min(const T& a, const Args&... args) -> T {
  return min({a, args...});
}

template <class T, class Compare, class... Args>
constexpr auto min(const T& a, const Compare& comp, const Args&... args) -> T {
  return min({a, args...}, comp);
}

template <class T, class Proj, class Compare, class... Args>
constexpr auto min(const T& a, const Compare& comp, const Proj& proj,
                   const Args&... args) -> T {
  return *min({a, args...}, std::move(comp), std::move(proj));
}

}  // namespace std
