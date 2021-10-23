#pragma once
#include "Symmetri/types.h"
#include <blockingconcurrentqueue.h>
#include <chrono>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <tuple>

namespace symmetri {
struct Model;
using Reducer = std::function<Model(Model)>;

using clock_t = std::chrono::system_clock;
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
    mutable std::vector<std::tuple<size_t, int64_t, int64_t, Transition>> log;
  };
  std::shared_ptr<shared> data;
  std::shared_ptr<moodycamel::BlockingConcurrentQueue<Transition>> transitions_;
};

Model run_all(Model model);
std::optional<Model> run_one(Model model);

} // namespace symmetri
