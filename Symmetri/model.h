#pragma once
#include <blockingconcurrentqueue.h>

#include <chrono>
#include <functional>
#include <memory>
#include <map>
#include <set>
#include <string>
#include <tuple>

#include "Symmetri/types.h"

namespace symmetri {
struct Model;
using Reducer = std::function<Model(Model)>;

using clock_t = std::chrono::system_clock;

using TaskInstance =
    std::tuple<clock_t::time_point,
               clock_t::time_point, size_t>;

struct Model {
  Model(const clock_t::time_point &t, const Marking &M,
        const TransitionMutation &Dm, const TransitionMutation &Dp,
        moodycamel::BlockingConcurrentQueue<Transition> &transitions)
      : data(std::make_shared<shared>(t, M, Dm, Dp)),
        transitions_(&transitions) {}
  struct shared {
    shared(const clock_t::time_point &t, const Marking &M,
           const TransitionMutation &Dm, const TransitionMutation &Dp)
        : timestamp(t), M(M), Dm(Dm), Dp(Dp) {}
    mutable clock_t::time_point timestamp;
    mutable Marking M;
    mutable std::set<Transition> active_transitions;
    const TransitionMutation Dm;
    const TransitionMutation Dp;
    std::multimap<Transition, TaskInstance> log;
    std::unordered_map<std::size_t, std::pair<Marking, Transitions>> cache;
  };
  std::shared_ptr<shared> data;
  moodycamel::BlockingConcurrentQueue<Transition> *transitions_;
};

Model run_all(Model model);
std::optional<Model> run_one(Model model);

}  // namespace symmetri
