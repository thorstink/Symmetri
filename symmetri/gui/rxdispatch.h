#pragma once

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

// The queue carries an edit reducer, a view reducer, an async Computer that
// resolves (off-thread) to an edit reducer, or an undo/redo request.
using Update = std::variant<model::EditReducer, model::ViewReducer,
                            model::Computer, model::Undo, model::Redo>;

// Optional hook fired after every push(), used to wake a blocked UI event loop
// so it can stay fully idle (no polling) until there is actually work to do.
// Set to e.g. glfwPostEmptyEvent; left null it is a no-op.
inline void (*on_push)() = nullptr;

void unsubscribe();
moodycamel::BlockingConcurrentQueue<Update>& getQueue();
void push(Update&& r);
void pushEdit(model::EditReducer r);
void pushView(model::ViewReducer r);
inline void pushUndo() { push(Update{model::Undo{}}); }
inline void pushRedo() { push(Update{model::Redo{}}); }

struct VisitPackage {
  auto operator()(model::EditReducer r) {
    return rpp::source::just(rximgui::rl, model::Action{std::move(r)})
        .as_dynamic();
  }
  auto operator()(model::ViewReducer r) {
    return rpp::source::just(rximgui::rl, model::Action{std::move(r)})
        .as_dynamic();
  }
  auto operator()(model::Computer f) {
    return (rpp::source::just(scheduler, std::move(f)) |
            rpp::operators::map(
                [](model::Computer f) { return model::Action{f()}; }))
        .as_dynamic();
  }
  auto operator()(model::Undo u) {
    return rpp::source::just(rximgui::rl, model::Action{u}).as_dynamic();
  }
  auto operator()(model::Redo r) {
    return rpp::source::just(rximgui::rl, model::Action{r}).as_dynamic();
  }

 private:
  inline static rpp::schedulers::thread_pool scheduler{4};
};

inline auto get_events_observable() {
  return rpp::source::create<Update>([](auto&& observer) {
           Update f;
           while (not observer.is_disposed()) {
             getQueue().wait_dequeue(f);
             observer.on_next(std::move(f));
           }
         }) |
         rpp::operators::subscribe_on(rpp::schedulers::new_thread{}) |
         rpp::operators::flat_map([](auto&& value) {
           // Move the variant into the visitor so its by-value Reducer/Computer
           // parameter is move-constructed (the std::function is sunk, not
           // copied). `value` is not used afterwards.
           return std::visit(VisitPackage(), std::move(value));
         });
}

}  // namespace rxdispatch
