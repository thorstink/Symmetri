#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "Symmetri/actions.h"
#include "Symmetri/polyaction.h"
#include "Symmetri/types.h"

namespace symmetri {

size_t calculateTrace(std::vector<Event> event_log) noexcept;
std::string printState(symmetri::TransitionState s) noexcept;

using Store = std::vector<std::pair<std::string, PolyAction>>;

struct Impl;
struct Application {
 private:
  mutable std::shared_ptr<Impl> impl;
  std::function<void(const std::string &t)> p;
  std::function<void(const clock_s::time_point &t)> publish;
  void createApplication(
      const symmetri::StateNet &net, const symmetri::NetMarking &m0,
      const std::optional<symmetri::NetMarking> &final_marking,
      const Store &store, const std::string &case_id,
      symmetri::StoppablePool &stp);

 public:
  Application(const std::set<std::string> &path_to_petri,
              const std::optional<symmetri::NetMarking> &final_marking,
              const Store &store, const std::string &case_id,
              symmetri::StoppablePool &stp);

  Application(const symmetri::StateNet &net, const symmetri::NetMarking &m0,
              const std::optional<symmetri::NetMarking> &final_marking,
              const Store &store, const std::string &case_id,
              symmetri::StoppablePool &stp);

  template <typename T>
  inline std::function<void(T)> registerTransitionCallback(
      const std::string &transition) const {
    return [transition, this](T) { p(transition); };
  }

  void doMeData(std::function<void()> f) const;

  std::tuple<clock_s::time_point, symmetri::Eventlog, symmetri::StateNet,
             symmetri::NetMarking, std::set<std::string>>
  get() const noexcept;

  const symmetri::StateNet &getNet();

  TransitionResult operator()() const noexcept;
  void togglePause();
};

}  // namespace symmetri
