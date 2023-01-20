#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "symmetri/actions.h"
#include "symmetri/polyaction.h"
#include "symmetri/types.h"

namespace symmetri {

size_t calculateTrace(const Eventlog &event_log) noexcept;
std::string printState(symmetri::TransitionState s) noexcept;

using Store = std::map<std::string, PolyAction>;

struct Impl;
struct Application {
 private:
  mutable std::shared_ptr<Impl> impl;
  std::function<void(const std::string &t)> p;
  std::function<void(const clock_s::time_point &t)> publish;
  void createApplication(
      const symmetri::StateNet &net, const symmetri::NetMarking &m0,
      const std::optional<symmetri::NetMarking> &final_marking,
      const Store &store,
      const std::vector<std::pair<symmetri::Transition, int8_t>> &priority,
      const std::string &case_id, const symmetri::StoppablePool &stp);

 public:
  Application(
      const std::set<std::string> &path_to_petri,
      const std::optional<symmetri::NetMarking> &final_marking,
      const Store &store,
      const std::vector<std::pair<symmetri::Transition, int8_t>> &priority,
      const std::string &case_id, const symmetri::StoppablePool &stp);

  Application(
      const symmetri::StateNet &net, const symmetri::NetMarking &m0,
      const std::optional<symmetri::NetMarking> &final_marking,
      const Store &store,
      const std::vector<std::pair<symmetri::Transition, int8_t>> &priority,
      const std::string &case_id, const symmetri::StoppablePool &stp);

  TransitionResult operator()() const noexcept;
  symmetri::Eventlog getEvenLog() const noexcept;
  std::vector<symmetri::Place> getMarking() const noexcept;
  std::vector<std::string> getActiveTransitions() const noexcept;
  std::function<void()> registerTransitionCallback(
      const std::string &transition) const noexcept;
  void togglePause() const noexcept;
  void exitEarly() const noexcept;
};

}  // namespace symmetri
