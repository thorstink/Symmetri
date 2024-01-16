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
