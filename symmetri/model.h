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
  Model(const StateNet &net, const Store &store, const NetMarking &M0)
      : net(net),
        store(store),
        timestamp(clock_s::now()),
        M(M0),
        active_transition_count(0) {
          pending_transitions.reserve(15);
        }

  Model &operator=(const Model &) { return *this; }
  Model(const Model &) = delete;

  const StateNet net;
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
