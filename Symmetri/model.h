#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <tuple>

#include "types.h"

namespace symmetri {

struct Model;
using Reducer = std::function<Model &(Model &&)>;

using TaskInstance =
    std::tuple<clock_t::time_point, clock_t::time_point, size_t>;

struct Model {
  Model(const clock_t::time_point &t, const StateNet &net, const NetMarking &M0)
      : timestamp(t), net(net), M(M0) {
    for (const auto &[transition, mut] : net) {
      transition_end_times[transition] = t;
    }
  }
  Model(const Model &) = delete;

  clock_t::time_point timestamp;
  StateNet net;
  NetMarking M;
  std::set<Transition> pending_transitions;
  std::multimap<Transition, TaskInstance> log;
  std::vector<Event> event_log;
  std::map<Transition, clock_t::time_point> transition_end_times;
};

std::tuple<Model &, Transitions> run_all(Model &model);

}  // namespace symmetri
