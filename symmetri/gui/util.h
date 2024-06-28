#pragma once
#include <tuple>
namespace rxu {

namespace detail {
template <class T>
using decay_t = typename std::decay<T>::type;

template <int Index>
struct take_at {
  template <class... ParamN>
  auto operator()(const ParamN&... pn) const ->
      typename std::tuple_element<Index, std::tuple<decay_t<ParamN>...>>::type {
    return std::get<Index>(std::tie(pn...));
  }
};

}  // namespace detail

template <int Index>
inline auto take_at() -> detail::take_at<Index> {
  return detail::take_at<Index>();
}

}  // namespace rxu

namespace symranges {
template <std::ranges::range R>
auto to_vector(R&& r) {
  std::vector<std::ranges::range_value_t<R>> v;

  // if we can get a size, reserve that much
  if constexpr (requires { std::ranges::size(r); }) {
    v.reserve(std::ranges::size(r));
  }

  // push all the elements
  for (auto&& e : r) {
    v.push_back(static_cast<decltype(e)&&>(e));
  }

  return v;
}

}  // namespace symranges
