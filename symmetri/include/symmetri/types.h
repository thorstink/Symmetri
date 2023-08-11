#pragma once
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

namespace symmetri {

using Place = std::string;       ///< The string representation of the place the
                                 ///< Petri net definition
using Transition = std::string;  ///< The string representation of the
                                 ///< transition the Petri net definition
using Clock =
    std::chrono::steady_clock;  ///< The clock definition in Symmetri is the
                                ///< steady clock for monotonic reasons

/**
 * @brief The difference kinds of results a transition can have.
 *
 */
enum class State {
  Scheduled,  ///< The transition is put into the task system
  Started,    ///< The transition started
  Completed,  ///< The transition completed as expected
  Deadlock,   ///< The transition deadlocked
  UserExit,   ///< The transition or interrupted and possibly stopped
  Paused,     ///< The transition is paused
  Error       ///< None of the above
};

/**
 * @brief This struct defines a subset of data that we associate with the
 * result of firing a transition.
 *
 */
struct Event {
  std::string case_id;      ///< The case_id of this event
  std::string transition;   ///< The transition that generated the event
  State state;              ///< The resulting state of the event
  Clock::time_point stamp;  ///< The timestamp when the reducer of this
                            ///< event was processed
};

using Eventlog = std::vector<Event>;  ///< The eventlog is simply a log of
                                      ///< events, sorted by their stamp
using Result =
    std::pair<Eventlog, State>;  ///< The result of a transition is described by
                                 ///< its Eventlog and State
using Net = std::unordered_map<
    Transition,
    std::pair<std::vector<Place>,
              std::vector<Place>>>;  ///< This is the class multiset definition
                                     ///< of a Petri net. For each transition
                                     ///< there is a pair of sets for input and
                                     ///< output transitions
using Marking =
    std::unordered_map<Place,
                       uint16_t>;  ///< The marking is limited from 0 to 65535
using PriorityTable =
    std::vector<std::pair<Transition, int8_t>>;  ///< Priority is limited from
                                                 ///< -128 to 127

}  // namespace symmetri
