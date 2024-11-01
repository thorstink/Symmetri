#pragma once

#include "draw.h"
#include "model.h"
#include "rpp/rpp.hpp"
#include "rxdispatch.h"

namespace rximgui {
rpp::subjects::publish_subject<int> framebus;
auto frameout = framebus.get_observer();
auto sendframe = []() { frameout.on_next(1); };
auto frames = framebus.get_observable();
inline rpp::schedulers::run_loop rl{};

}  // namespace rximgui

auto go() {
  using namespace rximgui;

  auto view_models =
      rxdispatch::get_events_observable() | rpp::operators::observe_on(rl) |
      rpp::operators::scan(
          model::Model{},
          [](auto &&m, const auto &f) {
            std::cout << "reduce " << std::this_thread::get_id() << std::endl;

            static size_t i = 0;
            std::cout << "update " << i++ << ", ref: " << m.data.use_count()
                      << std::endl;
            try {
              return f(std::move(m));
            } catch (const std::exception &e) {
              printf("%s", e.what());
              return std::move(m);
            }
          }) |
      rpp::operators::map(
          [](auto &&model) { return model::ViewModel(std::move(model)); });

  return frames |
         rpp::operators::with_latest_from(
             [](auto &&, auto &&vm) { return std::move(vm); },
             std::move(view_models)) |
         rpp::operators::subscribe_with_disposable(&drawUi);
}
