#pragma once
#include "task.hpp"
#include "types.h"
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <set>
#include <blockingconcurrentqueue.h>

namespace model {

struct Model;
using Reducer = symmetri::task<Model(Model)>;
using OptionalReducer = std::optional<Reducer>;
// using TransitionActionMap =
//     std::unordered_map<types::Transition, OptionalReducer(*)()>;
using TransitionActionMap =
    std::unordered_map<types::Transition, std::function<OptionalReducer()>>;

using clock_t = std::chrono::system_clock;
struct Model {
  Model(const clock_t::time_point &t, const types::Marking &M,
        const types::TransitionMutation &Dm,
        const types::TransitionMutation &Dp, moodycamel::BlockingConcurrentQueue<types::Transition>& transitions)
      : data(std::make_shared<shared>(t, M, Dm, Dp)), transitions_(&transitions) {}
  struct shared {
    shared(const clock_t::time_point &t, const types::Marking &M,
           const types::TransitionMutation &Dm,
           const types::TransitionMutation &Dp)
        : timestamp(t), M(M), Dm(Dm), Dp(Dp) {}
    mutable clock_t::time_point timestamp;
    mutable types::Marking M;
    mutable std::set<types::Transition> active_transitions;
    const types::TransitionMutation Dm;
    const types::TransitionMutation Dp;
    mutable std::vector<std::tuple<size_t, int64_t, int64_t, types::Transition>>
        log;
  };
  std::shared_ptr<shared> data;
  std::shared_ptr<moodycamel::BlockingConcurrentQueue<types::Transition>> transitions_;
};

Model run_all(Model model);
std::optional<Model> run_one(Model model);

} // namespace model
