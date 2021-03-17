#pragma once
#include "model.hpp"
#include "transitions.hpp"
#include <rxcpp/rx.hpp>

namespace actions {
using namespace transitions;
using namespace model;
using namespace rxcpp;

auto now() {
  return std::chrono::steady_clock::now().time_since_epoch().count();
}

template <typename Store>
auto executeTransition(const observe_on_one_worker &scheduler,
                       const Store &execute) {
  return [=](const Transition &transition) {
    return observable<>::just(transition, scheduler) |
           operators::map([execute](const Transition &transition) {
             const auto start = now();
             const auto model_update = execute(transition);
             const auto end = now();
             const auto thread_id =
                 std::hash<std::thread::id>{}(std::this_thread::get_id());
             return model_update.has_value()
                        ? Reducer([=, update = *model_update](Model model) {
                            model.data->M += model.data->Dp.at(transition);
                            model.data->log.emplace_back(
                                std::make_tuple(thread_id, start, end, transition));
                            return update(model);
                          })
                        : Reducer([=](Model model) {
                            model.data->M += model.data->Dp.at(transition);
                            model.data->log.emplace_back(
                                std::make_tuple(thread_id, start, end, transition));
                            return model;
                          });
           });
  };
}
} // namespace actions
