#pragma once
#include <chrono>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace symmetri {
using Place = std::string;
using Transition = std::string;
using clock_s = std::chrono::steady_clock;

enum class TransitionState { Started, Completed, Deadlock, UserExit, Error };

struct Event {
  std::string case_id, transition;
  TransitionState state;
  clock_s::time_point stamp;
  size_t thread_id;
};

using Eventlog = std::list<Event>;
using TransitionResult = std::pair<Eventlog, TransitionState>;
using StateNet =
    std::unordered_map<Transition,
                       std::pair<std::vector<Place>, std::vector<Place>>>;
using NetMarking = std::unordered_map<Place, uint16_t>;

template <typename T>
 TransitionResult runTransition(const T& x) {
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

bool MarkingReached(const NetMarking& marking, const NetMarking& final_marking);
bool StateNetEquality(const StateNet& net1, const StateNet& net2);
}  // namespace symmetri
