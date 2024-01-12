//                  ReactivePlusPlus library
//
//          Copyright Aleksey Loginov 2022 - present.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)
//
// Project home: https://github.com/victimsnino/ReactivePlusPlus
//

#pragma once

#include <rpp/observers/dynamic_observer.hpp>
#include <rpp/subscribers/specific_subscriber.hpp>

namespace rpp {
/**
 * \brief subscriber which uses dynamic_observer<T> to hide original callbacks
 * \tparam T type of values expected by this subscriber
 */
template <constraint::decayed_type T>
class dynamic_subscriber final
    : public specific_subscriber<T, dynamic_observer<T>> {
 public:
  using specific_subscriber<T, dynamic_observer<T>>::specific_subscriber;

  template <constraint::subscriber_of_type<T> TSub>
    requires(!std::is_same_v<std::decay_t<TSub>, dynamic_subscriber<T>>)
  dynamic_subscriber(const TSub& subscriber)
      : specific_subscriber<T, dynamic_observer<T>>{
            subscriber.get_subscription(), subscriber.get_observer()} {}
};

template <constraint::observer TObs>
dynamic_subscriber(TObs observer)
    -> dynamic_subscriber<utils::extract_observer_type_t<TObs>>;

template <constraint::observer TObs>
dynamic_subscriber(composite_subscription, TObs observer)
    -> dynamic_subscriber<utils::extract_observer_type_t<TObs>>;

template <typename T, typename Obs>
dynamic_subscriber(specific_subscriber<T, Obs>) -> dynamic_subscriber<T>;

template <typename OnNext, typename... Args>
dynamic_subscriber(composite_subscription, OnNext, Args...)
    -> dynamic_subscriber<std::decay_t<utils::function_argument_t<OnNext>>>;

template <typename OnNext, typename... Args>
dynamic_subscriber(OnNext, Args...)
    -> dynamic_subscriber<std::decay_t<utils::function_argument_t<OnNext>>>;
}  // namespace rpp
