#pragma once
#include <blockingconcurrentqueue.h>

#include <chrono>
#include <map>
#include <set>
#include <tuple>

#include "types.h"

namespace symmetri {
struct Model;
using Reducer = std::function<Model(Model)>;

using clock_t = std::chrono::system_clock;

using TaskInstance =
    std::tuple<clock_t::time_point, clock_t::time_point, size_t>;

struct Model {
  Model(const clock_t::time_point &t, const StateNet &net, const NetMarking &M0,
        moodycamel::BlockingConcurrentQueue<Transition> &transitions)
      : data(std::make_shared<shared>(t, net, M0)),
        transitions_(&transitions) {}
  struct shared {
    shared(const clock_t::time_point &t, const StateNet &net,
           const NetMarking &M)
        : timestamp(t), net(net), M(M) {}
    clock_t::time_point timestamp;
    const StateNet net;
    NetMarking M;
    std::set<Transition> active_transitions;
    std::vector<Transition> trace;
    std::multimap<Transition, TaskInstance> log;
  };
  std::shared_ptr<shared> data;
  moodycamel::BlockingConcurrentQueue<Transition> *transitions_;
};

Model run_all(Model model);

}  // namespace symmetri
