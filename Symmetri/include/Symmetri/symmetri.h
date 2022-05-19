#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "Symmetri/types.h"

namespace symmetri {

using clock_s = std::chrono::system_clock;

enum class TransitionState { Started, Completed, Deadlock, UserExit, Error };

struct Event {
  std::string case_id, transition;
  TransitionState state;
  clock_s::time_point stamp;
  size_t thread_id;
};

using Eventlog = std::vector<Event>;
using TransitionResult = std::pair<Eventlog, TransitionState>;

size_t calculateTrace(std::vector<Event> event_log);
std::string printState(symmetri::TransitionState s);

template <typename T>
constexpr TransitionResult runTransition(const T &x) {
  if constexpr (std::is_invocable_v<T>) {
    if constexpr (std::is_same_v<TransitionState, decltype(x())>) {
      return {{}, x()};
    } else if constexpr (std::is_same_v<TransitionResult, decltype(x())>) {
      return x();
    } else {
      x();
      return {{}, TransitionState::Completed};
    }
  } else {
    return {{}, TransitionState::Completed};
  }
}

class PolyAction {
 public:
  template <typename T>
  PolyAction(T x) : self_(std::make_shared<model<T>>(std::move(x))) {}

  friend TransitionResult runTransition(const PolyAction &x) {
    return x.self_->run_();
  }

 private:
  struct concept_t {
    virtual ~concept_t() = default;
    virtual TransitionResult run_() const = 0;
  };
  template <typename T>
  struct model final : concept_t {
    model(T x) : transition_function_(std::move(x)) {}
    TransitionResult run_() const override {
      return runTransition(transition_function_);
    }
    T transition_function_;
  };

  std::shared_ptr<const concept_t> self_;
};

symmetri::PolyAction retryFunc(const symmetri::PolyAction &f,
                               const symmetri::Transition &t,
                               const std::string &case_id,
                               unsigned int retry_count = 3);

using Store = std::unordered_map<std::string, PolyAction>;

struct Application {
 private:
  std::function<void(const std::string &t)> p;
  std::function<TransitionResult()> runApp;
  std::function<TransitionResult()> createApplication(
      const symmetri::StateNet &net, const symmetri::NetMarking &m0,
      const std::optional<symmetri::NetMarking> &final_marking,
      const Store &store, unsigned int thread_count, const std::string &case_id,
      bool interface);

 public:
  Application(const std::set<std::string> &path_to_petri,
              const std::optional<symmetri::NetMarking> &final_marking,
              const Store &store, unsigned int thread_count,
              const std::string &case_id = "NOCASE", bool use_webserver = true);
  Application(const symmetri::StateNet &net, const symmetri::NetMarking &m0,
              const std::optional<symmetri::NetMarking> &final_marking,
              const Store &store, unsigned int thread_count,
              const std::string &case_id, bool interface);

  template <typename T>
  inline std::function<void(T)> push(const std::string &transition) const {
    return [transition, this](T) { p(transition); };
  }

  TransitionResult operator()() const;
};

}  // namespace symmetri
