#pragma once

#include <memory>

#include "draw.h"
#include "model.h"
#include "rpp/rpp.hpp"
#include "rxdispatch.h"
#include "rximgui.h"

// The reduce-step state: the current Model plus the undo history. Held behind a
// copyable shared_ptr because rpp::scan copies its cached seed (and Model is
// move-only); the state itself is mutated in place through the handle.
struct ReduceState {
  model::Model model;
  model::History history;
};

auto go() {
  using namespace rximgui;

  auto view_models =
      rxdispatch::get_events_observable() | rpp::operators::observe_on(rl) |
      rpp::operators::scan(
          std::make_shared<ReduceState>(),
          [](std::shared_ptr<ReduceState> &&s, const model::Action &a) {
            try {
              // Edit reducers transform EditState and are recorded in the undo
              // history; view reducers mutate ViewState in place (may read
              // EditState); Undo/Redo replay the history. View state is never
              // touched by undo, so selection/zoom survive.
              if (auto *e = std::get_if<model::EditReducer>(&a)) {
                s->model.edit = s->history.apply(*e, std::move(s->model.edit));
              } else if (auto *v = std::get_if<model::ViewReducer>(&a)) {
                (*v)(s->model.view, s->model.edit);
              } else if (std::get_if<model::Undo>(&a)) {
                if (s->history.canUndo()) {
                  s->model.edit = s->history.undo();
                  // Selection/highlights index into the net, which just
                  // changed underneath them; drop them to avoid stale indices.
                  model::clearSelection(s->model.view);
                }
              } else if (std::get_if<model::Redo>(&a)) {
                if (s->history.canRedo()) {
                  s->model.edit = s->history.redo();
                  model::clearSelection(s->model.view);
                }
              }
            } catch (const std::exception &e) {
              printf("%s", e.what());
            }
            // `s` is an rvalue-ref parameter: C++20 moves it on return.
            return s;
          }) |
      rpp::operators::map([](const auto &s) {
        return std::make_shared<model::ViewModel>(s->model);
      });

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
