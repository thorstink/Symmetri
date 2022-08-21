#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "Symmetri/polyaction.h"
#include "Symmetri/types.h"

namespace symmetri {

size_t calculateTrace(std::vector<Event> event_log);
std::string printState(symmetri::TransitionState s);

using Store = std::unordered_map<std::string, PolyAction>;

struct Impl;
struct Application {
 private:
  mutable std::shared_ptr<Impl> impl;
  std::function<void(const std::string &t)> p;
  void createApplication(
      const symmetri::StateNet &net, const symmetri::NetMarking &m0,
      const std::optional<symmetri::NetMarking> &final_marking,
      const Store &store, unsigned int thread_count,
      const std::string &case_id);

 public:
  Application(const std::set<std::string> &path_to_petri,
              const std::optional<symmetri::NetMarking> &final_marking,
              const Store &store, unsigned int thread_count,
              const std::string &case_id);
  Application(const symmetri::StateNet &net, const symmetri::NetMarking &m0,
              const std::optional<symmetri::NetMarking> &final_marking,
              const Store &store, unsigned int thread_count,
              const std::string &case_id);

  template <typename T>
  inline std::function<void(T)> registerTransitionCallback(
      const std::string &transition) const {
    return [transition, this](T) { p(transition); };
  }

  std::tuple<clock_s::time_point, symmetri::Eventlog, symmetri::StateNet,
             symmetri::NetMarking, std::set<std::string>>
  get() const;
  TransitionResult operator()() const;
  void togglePause();
};

}  // namespace symmetri
