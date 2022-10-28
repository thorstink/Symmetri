#pragma once

#include <blockingconcurrentqueue.h>

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <tuple>

#include "Symmetri/symmetri.h"

namespace symmetri {

struct Model;
using Reducer = std::function<Model &(Model &&)>;
PolyAction getTransition(const Store &s, const std::string &t);

Reducer runTransition(const std::string &T_i, const PolyAction &task,
                      const std::string &case_id,
                      const clock_s::time_point &queue_time = clock_s::now());

struct Model {
  Model(const StateNet &net, const Store &store, const NetMarking &M0)
      : net(net), store(store), timestamp(clock_s::now()), M(M0) {}

  Model &operator=(const Model &x) { return *this; }
  Model(const Model &) = delete;

  const StateNet net;
  const Store &store;

  clock_s::time_point timestamp;
  NetMarking M;
  std::set<Transition> pending_transitions;
  Eventlog event_log;
};

Model &runAll(
    Model &model, moodycamel::BlockingConcurrentQueue<Reducer> &reducers,
    moodycamel::BlockingConcurrentQueue<PolyAction> &polymorphic_actions,
    const std::string &case_id = "undefined_case_id");

}  // namespace symmetri
