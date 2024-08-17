#pragma once

/** @file types.h */

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

#include "symmetri/colors.hpp"
namespace symmetri {
using Place = std::string;       ///< The string representation of the place the
                                 ///< Petri net definition
using Transition = std::string;  ///< The string representation of the
                                 ///< transition the Petri net definition
using Clock =
    std::chrono::steady_clock;  ///< The clock definition in Symmetri is the
                                ///< steady clock for monotonic reasons

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
    std::pair<std::vector<std::pair<Place, Token>>,
              std::vector<std::pair<Place, Token>>>>;  ///< This is the multiset
                                                       ///< definition of a
                                                       ///< Petri net. For each
                                                       ///< transition there is
                                                       ///< a pair of lists for
                                                       ///< colored input and
                                                       ///< output places

using Marking =
    std::vector<std::pair<Place, Token>>;  ///< The Marking is a vector pairs in
                                           ///< which the Place describes in
                                           ///< which place the Token is.

using PriorityTable =
    std::vector<std::pair<Transition, int8_t>>;  ///< Priority is limited from
                                                 ///< -128 to 127

/**
 * @brief A DirectMutation is a synchronous no-operation function. It simply
 * mutates the mutation on the petri net executor loop. This way the deferring
 * to the threadpool and back to the petri net loop is avoided.
 *
 */
struct DirectMutation {};
class PetriNet;

/**
 * @brief A DirectMutation is a synchronous Callback that always
 * completes.
 *
 */
bool isSynchronous(const DirectMutation &);

/**
 * @brief fire for a direct mutation does no work at all.
 *
 * @return Token
 */
Token fire(const DirectMutation &);

/**
 * @brief The fire specialization for a PetriNet runs the Petri net until it
 * completes, deadlocks or is preempted. It returns an event log along with the
 * result state.
 *
 * @return Token
 */
Token fire(const PetriNet &);

/**
 * @brief The cancel specialization for a PetriNet breaks the PetriNets'
 * internal loop. It will not queue any new Callbacks and it will cancel all
 * child Callbacks that are running. The cancel function will return before the
 * PetriNet is preempted completely.
 *
 * @return void
 */
void cancel(const PetriNet &);

/**
 * @brief The pause specialization for a PetriNet prevents new fire-able
 * Callbacks from being scheduled. It will also pause all child Callbacks that
 * are running. The pause function will return before the PetriNet will pause.
 *
 */
void pause(const PetriNet &);

/**
 * @brief The resume specialization for a PetriNet undoes pause and puts the
 * PetriNet back in a normal state where all fireable Callback are scheduled for
 * execution. The resume function will return before the PetriNet will resume.
 *
 */
void resume(const PetriNet &);

/**
 * @brief Get the Log object
 *
 * @return Eventlog
 */
Eventlog getLog(const PetriNet &);

}  // namespace symmetri
