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
           while (not observer.is_disposed()) {
             std::cout << "dispatch " << mainthreadid << std::endl;
             getQueue().wait_dequeue(f);
             observer.on_next(std::move(f));
           }
         }) |
         rpp::operators::subscribe_on(rpp::schedulers::thread_pool{3});
}

}  // namespace rxdispatch
