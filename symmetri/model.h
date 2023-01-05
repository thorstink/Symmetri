#pragma once

#include <blockingconcurrentqueue.h>

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <tuple>

#include "symmetri/symmetri.h"

namespace symmetri {

struct Model;
using Reducer = std::function<Model &(Model &&)>;
PolyAction getTransition(const Store &s, const std::string &t);

Reducer runTransition(const std::string &T_i, const PolyAction &task,
                      const std::string &case_id);

struct Model {
  Model(const StateNet &net, const Store &store,
        std::vector<std::pair<symmetri::Transition, uint8_t>> _priority,
        const NetMarking &M0)
      : net(net),
        priority(_priority),
        store(store),
        timestamp(clock_s::now()),
        M(M0),
        active_transition_count(0) {
    // sort priority for lookup:
    std::sort(priority.begin(), priority.end(),
              [](auto a, auto b) { return a.first < b.first; });

    // reserve some heap memory for pending transtitions.
    pending_transitions.reserve(15);
    // create reverse lookup tables
    for (auto [place, c] : M0) {
      for (int i = 0; i < c; i++) {
        tokens.push_back(place);
      }

      std::vector<Transition> transitions;
      for (const auto &[transition, io_places] : net) {
        for (const auto &input_place : io_places.first) {
          if (place == input_place) {
            transitions.push_back(transition);
          }
        }
      }
      if (!transitions.empty()) {
        reverse_loopup.push_back({place, transitions});
      }
    }
    std::sort(reverse_loopup.begin(), reverse_loopup.end(),
              [](const auto &a, const auto &b) { return a.first < b.first; });
  }

  Model &operator=(const Model &) { return *this; }
  Model(const Model &) = delete;

  const StateNet net;
  std::vector<Place> tokens;
  std::vector<std::pair<symmetri::Transition, uint8_t>> priority;
  std::vector<std::pair<Place, std::vector<Transition>>> reverse_loopup;
  const Store &store;

  clock_s::time_point timestamp;
  NetMarking M;
  std::vector<Transition> pending_transitions;
  uint16_t active_transition_count;
  Eventlog event_log;
};

Model &runAll(Model &model,
              moodycamel::BlockingConcurrentQueue<Reducer> &reducers,
              const StoppablePool &polymorphic_actions,
              const std::string &case_id = "undefined_case_id");

}  // namespace symmetri
