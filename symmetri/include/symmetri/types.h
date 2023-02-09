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

/**
 * @brief The difference kinds of results a transition can have.
 *
 */
enum class State {
  Started,    /// The transition started
  Completed,  /// The transition completed as expected
  Deadlock,   /// The transition deadlocked (e.g. it was a petri net)
  UserExit,   /// The transition or interupted and was stopped
  Error       /// None of the above
};

/**
 * @brief This struct defines a subset of data that we associate with the
 * payload of each transition.
 *
 */
struct Event {
  std::string case_id;
  std::string transition;
  State state;
  clock_s::time_point stamp;
  size_t thread_id;
};

using Eventlog = std::list<Event>;
using Result = std::pair<Eventlog, State>;
using Net =
    std::unordered_map<Transition,
                       std::pair<std::vector<Place>, std::vector<Place>>>;
using Marking = std::unordered_map<Place, uint16_t>;

/**
 * @brief Checks if the transition-function can be invoked.
 *
 * @tparam T The type of the transition.
 * @return true The pre- and post-marking-mutation can happen instantly.
 * @return false The pre-marking mutation happens instantly and the
 * post-marking-mutation should only happen after the transition is invoked.
 */
template <typename T>
bool constexpr isDirectTransition(const T&) {
  return !std::is_invocable_v<T>;
}

/**
 * @brief Generates a Result based on what kind of information the
 * transition-function returns.
 *
 * @tparam T The transition-function type.
 * @param transition The function to be executed.
 * @return Result Contains information on the result-state and
 * possible eventlog of the transition.
 */
template <typename T>
Result runTransition(const T& transition) {
  if constexpr (!std::is_invocable_v<T>) {
    return {{}, State::Completed};
  } else if constexpr (std::is_same_v<State, decltype(transition())>) {
    return {{}, transition()};
  } else if constexpr (std::is_same_v<Result, decltype(transition())>) {
    return transition();
  } else {
    transition();
    return {{}, State::Completed};
  }
}

/**
 * @brief Checks if the markings are exactly the same.
 *
 * @tparam T
 * @param m1
 * @param m2
 * @return true
 * @return false
 */
template <typename T>
bool MarkingEquality(const std::vector<T>& m1, const std::vector<T>& m2);

/**
 * @brief Checks if marking is at least at least a subset of final_marking.
 *
 * @tparam T
 * @param marking
 * @param final_marking
 * @return true
 * @return false
 */
template <typename T>
bool MarkingReached(const std::vector<T>& marking,
                    const std::vector<T>& final_marking);

/**
 * @brief Checks if two petri-nets have the equal amount of arcs between places
 * and transitions of the same name.
 *
 * @param net1
 * @param net2
 * @return true
 * @return false
 */
bool StateNetEquality(const Net& net1, const Net& net2);
}  // namespace symmetri
