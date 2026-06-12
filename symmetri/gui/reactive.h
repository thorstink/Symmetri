#pragma once

#include <iostream>
#include <memory>

#include "draw.h"
#include "model.h"
#include "rpp/rpp.hpp"
#include "rxdispatch.h"
#include "rximgui.h"

auto go() {
  using namespace rximgui;

  auto view_models =
      rxdispatch::get_events_observable() | rpp::operators::observe_on(rl) |
      rpp::operators::scan(
          // Model is move-only, but rpp::scan requires a copy-constructible
          // seed (it copies the cached seed into its observer strategy). Hold
          // the model behind a copyable shared_ptr handle; the model itself is
          // still threaded through the reducers strictly by move.
          std::make_shared<model::Model>(),
          [](std::shared_ptr<model::Model> &&m, const auto &f) {
            std::cout << "reduce " << std::this_thread::get_id() << std::endl;

            static size_t i = 0;
            std::cout << "update " << i++ << ", ref: " << m.use_count()
                      << std::endl;
            try {
              *m = f(std::move(*m));
            } catch (const std::exception &e) {
              printf("%s", e.what());
            }
            // `m` is an rvalue-ref parameter: C++20 moves it on return,
            // so an explicit std::move would be redundant.
            return m;
          }) |
      rpp::operators::map(
          [](const auto &m) { return std::make_shared<model::ViewModel>(*m); });

  return frames |
         // The latest view-model is cached and re-combined with every frame
         // tick, so it must be copyable. ViewModel is move-only (and a fat
         // snapshot of many vectors); hold it behind a shared_ptr handle so a
         // frame costs a refcount bump instead of rebuilding/copying it.
         rpp::operators::with_latest_from(
             [](auto &&, const auto &vm) { return vm; },
             std::move(view_models)) |
         rpp::operators::subscribe_with_disposable(&drawUi);
}
