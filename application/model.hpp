#pragma once
#include "application.hpp"
#include "transitions.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <tuple>

namespace model {

struct Model;
using Transitions = std::vector<transitions::Transition>;
using Reducer = std::function<Model(Model)>;

using namespace application;
struct Model {
  Model(const std::chrono::steady_clock::time_point &t, const Marking &M,
        const TransitionMutation &Dm, const TransitionMutation &Dp)
      : data(std::make_shared<shared>(t, M, Dm, Dp)) {}
  struct shared {
    shared(const std::chrono::steady_clock::time_point &t, const Marking &M,
           const TransitionMutation &Dm, const TransitionMutation &Dp)
        : timestamp(t), M(M), Dm(Dm), Dp(Dp) {}
    mutable std::chrono::steady_clock::time_point timestamp;
    mutable Marking M;
    const TransitionMutation Dm;
    const TransitionMutation Dp;
    mutable std::vector<std::tuple<size_t, int64_t, int64_t, transitions::Transition>> log;
  };
  std::shared_ptr<shared> data;
};

std::pair<Model, Transitions> run(Model model);

} // namespace model
