#pragma once

#include <chrono>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace symmetri {
using Transition = std::string;
using Place = std::string;
using Transitions = std::vector<Transition>;
using clock_t = std::chrono::system_clock;

enum class TransitionState { Started, Completed, Error };

inline std::string printState(TransitionState s) {
  return s == TransitionState::Started     ? "Started"
         : s == TransitionState::Completed ? "Completed"
                                           : "Error";
}

using Event =
    std::tuple<std::string, Transition, TransitionState, clock_t::time_point>;

using StateNet =
    std::map<Transition, std::pair<std::multiset<Place>, std::multiset<Place>>>;
using NetMarking = std::map<Place, uint16_t>;

}  // namespace symmetri
