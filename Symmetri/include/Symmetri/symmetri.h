#pragma once

#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <variant>

namespace symmetri {

struct Application;
using Transition = std::string;
using clock_t = std::chrono::system_clock;

enum class TransitionState { Started, Completed, Error };

using Event =
    std::tuple<std::string, Transition, TransitionState, clock_t::time_point>;

typedef std::vector<Event> (*loggedFunction)();
typedef void (*nonLoggedFunction)();

using Action = std::variant<loggedFunction, nonLoggedFunction, Application>;

size_t calculateTrace(std::vector<Event> event_log);
inline std::string printState(symmetri::TransitionState s) {
  return s == symmetri::TransitionState::Started     ? "Started"
         : s == symmetri::TransitionState::Completed ? "Completed"
                                                     : "Error";
}

template <typename T>
constexpr TransitionState run(const T &x) {
  if constexpr (std::is_same_v<void, decltype(x())>) {
    x();
    return TransitionState::Completed;
  } else {
    return x();
  }
}

class object_t {
 public:
  object_t() {}
  template <typename T>
  object_t(T x) : self_(std::make_shared<model<T>>(std::move(x))) {}

  friend TransitionState run(const object_t &x) { return x.self_->run_(); }

 private:
  struct concept_t {
    virtual ~concept_t() = default;
    virtual TransitionState run_() const = 0;
  };
  template <typename T>
  struct model final : concept_t {
    model(T x) : data_(std::move(x)) {}
    TransitionState run_() const override { return run(data_); }

    T data_;
  };

  std::shared_ptr<const concept_t> self_;
};

using TransitionActionMap = std::unordered_map<std::string, symmetri::object_t>;

struct Application {
 private:
  std::function<void(const std::string &t)> p;
  std::function<std::vector<Event>()> runApp;

 public:
  Application(const std::set<std::string> &path_to_petri,
              const TransitionActionMap &store, unsigned int thread_count,
              const std::string &case_id = "NOCASE", bool use_webserver = true);
  std::vector<Event> operator()() const;

  template <typename T>
  inline std::function<void(T)> push(const std::string &transition) const {
    return [transition, this](T) { p(transition); };
  }
};

}  // namespace symmetri
