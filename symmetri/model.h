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
  Model(const StateNet &_net, const Store &store,
        std::vector<std::pair<symmetri::Transition, int8_t>> _priority,
        const NetMarking &M0)
      : store(store), timestamp(clock_s::now()), active_transition_count(0) {
    // populate net:
    const auto net_size = _net.size();
    net.transition.reserve(net_size);
    net.input.reserve(net_size);
    net.output.reserve(net_size);
    for (const auto &[t, io] : _net) {
      net.transition.push_back(t);
      net.input.push_back(io.first);
      net.output.push_back(io.second);
    }

    // sort priority so the vectors are sorted
    std::sort(_priority.begin(), _priority.end(),
              [](auto a, auto b) { return a.first < b.first; });
    for (const auto &[t, p] : _priority) {
      priority.first.push_back(t);
      priority.second.push_back(p);
    }

    // reserve some heap memory for pending transtitions.
    pending_transitions.reserve(15);

    // create reverse lookup table
    std::vector<Place> unique_input_places;
    for (size_t c = 0; c < net_size; c++) {
      unique_input_places.insert(unique_input_places.begin(),
                                 net.input[c].begin(), net.input[c].end());
    }
    // sort and remove duplicates.
    std::sort(unique_input_places.begin(), unique_input_places.end());
    auto last =
        std::unique(unique_input_places.begin(), unique_input_places.end());
    unique_input_places.erase(last, unique_input_places.end());

    // create a reverse lookup map that associates a place to the transitions to
    // which it is an input place.
    for (const auto &place : unique_input_places) {
      std::vector<Transition> transitions;
      for (size_t c = 0; c < net_size; c++) {
        for (const auto &input_place : net.input[c]) {
          if (place == input_place) {
            transitions.push_back(net.transition[c]);
          }
        }
      }

      if (!transitions.empty()) {
        reverse_loopup.first.push_back(place);
        reverse_loopup.second.push_back(transitions);
      }
    }

    // populate initial marking
    for (auto [place, c] : M0) {
      for (int i = 0; i < c; i++) {
        tokens.push_back(place);
      }
    }
  }

  Model &operator=(const Model &) { return *this; }
  Model(const Model &) = delete;

  struct {
    std::vector<Transition> transition;
    std::vector<std::vector<Place>> input, output;
  } net;

  std::vector<Place> tokens;
  std::pair<std::vector<Transition>, std::vector<int8_t>> priority;
  std::pair<std::vector<Place>, std::vector<std::vector<Transition>>>
      reverse_loopup;
  const Store &store;

  clock_s::time_point timestamp;
  std::vector<Transition> pending_transitions;
  uint16_t active_transition_count;
  Eventlog event_log;
};

Model &runAll(Model &model,
              moodycamel::BlockingConcurrentQueue<Reducer> &reducers,
              const StoppablePool &polymorphic_actions,
              const std::string &case_id = "undefined_case_id");

}  // namespace symmetri
