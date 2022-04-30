#pragma once

#include <blockingconcurrentqueue.h>

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
  Model(const clock_t::time_point &t, const StateNet &net,
        const std::unordered_map<std::string, symmetri::object_t> &store,
        const NetMarking &M0)
      : net(net), store(store), timestamp(t), M(M0) {
    for (const auto &[transition, mut] : net) {
      transition_end_times[transition] = t;
    }
  }

  Model &operator=(const Model &x) { return *this; }

  Model(const Model &) = delete;

  const StateNet net;
  const std::unordered_map<std::string, symmetri::object_t> store;

  // this is a random padding struct.. it seems to improve latency between
  // transitions. Need to do it prettier/research it.
  std::array<int, 10> pad;

  clock_t::time_point timestamp;
  NetMarking M;
  std::set<Transition> pending_transitions;
  std::multimap<Transition, TaskInstance> log;
  std::vector<Event> event_log;
  std::map<Transition, clock_t::time_point> transition_end_times;
};

Model &run_all(
    Model &model, moodycamel::BlockingConcurrentQueue<Reducer> &reducers,
    moodycamel::BlockingConcurrentQueue<object_t> &polymorphic_actions,
    const std::string &case_id);

}  // namespace symmetri
