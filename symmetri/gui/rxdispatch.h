#pragma once

#include <iostream>

#include "model.h"
#include "rpp/rpp.hpp"
namespace rxdispatch {
void unsubscribe();
moodycamel::BlockingConcurrentQueue<model::Reducer>& getQueue();
void push(model::Reducer&& r);

inline auto get_events_observable() {
  return rpp::source::create<model::Reducer>([](auto&& observer) {
    model::Reducer f;
    auto mainthreadid = std::this_thread::get_id();
    std::cout << "dispatch " << mainthreadid << std::endl;
    while (not observer.is_disposed()) {
      getQueue().wait_dequeue(f);
      observer.on_next(std::move(f));
    }
  });
}

}  // namespace rxdispatch
