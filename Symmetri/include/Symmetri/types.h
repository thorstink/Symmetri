#pragma once
#include <immer/vector.hpp>
#include <map>
#include <string>
#include <vector>

namespace symmetri {
using Place = std::string;
using Transition = std::string;
using clock_s = std::chrono::system_clock;

enum class TransitionState { Started, Completed, Deadlock, UserExit, Error };

struct Event {
  std::string case_id, transition;
  TransitionState state;
  clock_s::time_point stamp;
  size_t thread_id;
};

using Eventlog = immer::vector<Event>;
using TransitionResult = std::pair<Eventlog, TransitionState>;

using StateNet =
    std::map<Transition, std::pair<std::vector<Place>, std::vector<Place>>>;
using NetMarking = std::map<Place, uint16_t>;

inline bool MarkingReached(const NetMarking& marking,
                           const NetMarking& final_marking) {
  return std::all_of(
      std::begin(final_marking), std::end(final_marking),
      [&](const auto& p_m) { return marking.at(p_m.first) >= p_m.second; });
}

inline bool StateNetEquality(const StateNet& net1, const StateNet& net2) {
  if (net1.size() != net2.size()) {
    return false;
  }
  for (const auto& [t1, mut1] : net1) {
    if (net2.contains(t1)) {
      for (const auto& pre : mut1.first) {
        if (mut1.first.size() != net2.at(t1).first.size()) {
          return false;
        }
        if (std::count(std::begin(mut1.first), std::end(mut1.first), pre) !=
            std::count(std::begin(net2.at(t1).first),
                       std::end(net2.at(t1).first), pre)) {
          return false;
        }
      }
      for (const auto& post : mut1.second) {
        if (mut1.second.size() != net2.at(t1).second.size()) {
          return false;
        }
        if (std::count(std::begin(mut1.second), std::end(mut1.second), post) !=
            std::count(std::begin(net2.at(t1).second),
                       std::end(net2.at(t1).second), post)) {
          return false;
        }
      }
    } else {
      return false;
    }
  }

  return true;
}
}  // namespace symmetri
