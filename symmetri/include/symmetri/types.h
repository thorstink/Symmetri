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

/**
 * @brief Checks if the markings are exactly the same. Note that this uses a
 * different type for Marking compared to the Marking type used to
 * construct a net (an unordered map of strings). In this format the amount of
 * tokens in a particular place is represented by how often that place occurs in
 * the vector. For example: {"A","A","B"} is a marking with two tokens in place
 * "A" and one token in place "B". This format does not have the overhead of
 * mentioning all empty places.
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
 * @brief Checks if marking is at least a subset of final_marking. Note
 * that this uses a different type for Marking compared to the Marking
 * type used to construct a net (an unordered map of strings). In this
 * format the amount of tokens in a particular place is represented by how often
 * that place occurs in the vector. For example: {"A","A","B"} is a marking with
 * two tokens in place "A" and one token in place "B". This format does not have
 * the overhead of mentioning all empty places.
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
 * @brief Checks if two petri-nets have equal amount of arcs between places
 * and transitions of the same name.
 *
 * @param net1
 * @param net2
 * @return true
 * @return false
 */
bool stateNetEquality(const Net& net1, const Net& net2);

/**
 * @brief Calculates a hash given an event log. This hash is only influenced by
 * the order of the completions of transitions and if the output of those
 * transitions is Completed, or something else.
 *
 * @param event_log An eventlog, can both be from a terminated or a still active
 * net.
 * @return size_t The hashed result.
 */
size_t calculateTrace(const Eventlog& event_log) noexcept;

/**
 * @brief A convenience function to get a string representation of the
 * state-enum.
 *
 * @param s The State
 * @return std::string The State as a human readable string.
 */
std::string printState(State s) noexcept;

}  // namespace symmetri
