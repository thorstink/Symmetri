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

#include <rpp/defs.hpp>  // RPP_NO_UNIQUE_ADDRESS
#include <rpp/operators/details/subscriber_with_state.hpp>  // create_subscriber_with_state
#include <rpp/operators/fwd/scan.hpp>                       // own forwarding
#include <rpp/operators/lift.hpp>    // required due to operator uses lift
#include <rpp/operators/reduce.hpp>  // reduce to re-use
#include <rpp/subscribers/constraints.hpp>  // constraint::subscriber
#include <rpp/utils/functors.hpp>           // forwarding_on_error
#include <rpp/utils/utilities.hpp>          // utils::as_const

IMPLEMENTATION_FILE(scan_tag);

namespace rpp::details {
struct scan_on_next : private reduce_on_next {
  template <constraint::decayed_type Result, typename AccumulatorFn>
  void operator()(auto&& value, const constraint::subscriber auto& sub,
                  const reduce_state<Result, AccumulatorFn>& state) const {
    reduce_on_next::operator()(std::forward<decltype(value)>(value), sub,
                               state);
    sub.on_next(utils::as_const(state.seed));
  }
};

template <constraint::decayed_type Type, constraint::decayed_type Result,
          scan_accumulator<Result, Type> AccumulatorFn>
struct scan_impl {
  Result initial_value;
  RPP_NO_UNIQUE_ADDRESS AccumulatorFn accumulator;

  template <constraint::subscriber_of_type<Result> TSub>
  auto operator()(TSub&& subscriber) const {
    auto subscription = subscriber.get_subscription();
    // dynamic_state there to make shared_ptr for observer instead of making
    // shared_ptr for state
    return create_subscriber_with_dynamic_state<Type>(
        std::move(subscription), scan_on_next{}, utils::forwarding_on_error{},
        utils::forwarding_on_completed{}, std::forward<TSub>(subscriber),
        reduce_state<Result, AccumulatorFn>{initial_value, accumulator});
  }
};
}  // namespace rpp::details
