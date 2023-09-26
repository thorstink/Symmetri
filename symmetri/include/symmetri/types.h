#pragma once

/** @file types.h */

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

#include "result_factory.hpp"
namespace symmetri {

const size_t PLACEHOLDER = 999;
const std::string PLACEHOLDER_STRING = "PLACEHOLDER";

using Place = std::string;       ///< The string representation of the place the
                                 ///< Petri net definition
using Transition = std::string;  ///< The string representation of the
                                 ///< transition the Petri net definition
using Clock =
    std::chrono::steady_clock;  ///< The clock definition in Symmetri is the
                                ///< steady clock for monotonic reasons

namespace state {
const static Token Scheduled(registerToken("Scheduled"));
const static Token Started(registerToken("Started"));
const static Token Completed(registerToken("Completed"));
const static Token Deadlock(registerToken("Deadlock"));
const static Token UserExit(registerToken("UserExit"));
const static Token Paused(registerToken("Paused"));
const static Token Error(registerToken("Error"));
};  // namespace state

/**
 * @brief This struct defines a subset of data that we associate with the
 * result of firing a transition.
 *
 */
struct Event {
  std::string case_id;      ///< The case_id of this event
  std::string transition;   ///< The transition that generated the event
  Token state;              ///< The result of the event
  Clock::time_point stamp;  ///< The timestamp of the event
};

using Eventlog = std::vector<Event>;  ///< The eventlog is simply a log of
                                      ///< events, sorted by their stamp

using Net = std::unordered_map<
    Transition,
    std::pair<std::vector<Place>,
              std::vector<Place>>>;  ///< This is the multiset definition
                                     ///< of a Petri net. For each transition
                                     ///< there is a pair of lists for input and
                                     ///< output transitions

using Marking = std::vector<std::pair<std::string, std::string>>;

using PriorityTable =
    std::vector<std::pair<Transition, int8_t>>;  ///< Priority is limited from
                                                 ///< -128 to 127

}  // namespace symmetri
