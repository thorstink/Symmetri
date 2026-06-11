#pragma once

#include <array>
#include <functional>
#include <utility>
#include <variant>

#include "blockingconcurrentqueue.h"
#include "model.h"
#include "rpp/observables/observable.hpp"
#include "rpp/operators/flat_map.hpp"
#include "rpp/operators/fwd.hpp"
#include "rpp/operators/map.hpp"
#include "rpp/operators/subscribe_on.hpp"
#include "rpp/rpp.hpp"
#include "rpp/schedulers/new_thread.hpp"
#include "rpp/schedulers/run_loop.hpp"
#include "rpp/schedulers/thread_pool.hpp"
#include "rpp/sources/create.hpp"
#include "rpp/sources/from.hpp"
#include "rximgui.h"

namespace rxdispatch {

using Update = std::variant<model::Reducer, model::Computer>;

void unsubscribe();
moodycamel::BlockingConcurrentQueue<Update>& getQueue();
void push(Update&& r);

struct VisitPackage {
  auto operator()(model::Reducer r) {
    return rpp::source::just(rximgui::rl, std::move(r)).as_dynamic();
  }
  auto operator()(model::Computer f) {
    return (rpp::source::just(scheduler, std::move(f)) |
            rpp::operators::map([](auto f) { return f(); }))
        .as_dynamic();
  }

 private:
  inline static rpp::schedulers::thread_pool scheduler{4};
};

inline auto get_events_observable() {
  return rpp::source::create<Update>([](auto&& observer) {
           Update f;
           while (not observer.is_disposed()) {
             getQueue().wait_dequeue(f);
             observer.on_next(std::forward<Update>(f));
           }
         }) |
         rpp::operators::subscribe_on(rpp::schedulers::new_thread{}) |
         rpp::operators::flat_map(
             [](auto&& value) { return std::visit(VisitPackage(), value); });
}

}  // namespace rxdispatch
