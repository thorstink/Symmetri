#pragma once

/** @file symmetri.h */

#include <set>
#include <unordered_map>

#include "symmetri/callback.h"
#include "symmetri/tasks.h"
#include "symmetri/types.h"

/**
 * @brief The PetriNet class is a class that can create, configure and
 * run a Petri net.
 *
 */
class PetriNet;

/**
 * @brief The fire specialization for a PetriNet runs the Petri net until it
 * completes, deadlocks or is preempted. It returns an event log along with the
 * result state.
 *
 * @return symmetri::State
 */
symmetri::State fire(const PetriNet &);

/**
 * @brief The cancel specialization for a PetriNet breaks the PetriNets'
 * internal loop. It will not queue any new Callbacks and it will cancel all
 * child Callbacks that are running. The cancel function will return before the
 * PetriNet is preempted completely.
 *
 * @return symmetri::Result
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
 * @return symmetri::Eventlog
 */
symmetri::Eventlog getLog(const PetriNet &);

namespace symmetri {
/**
 * @brief A Store is a mapping from Transitions, represented by a string that is
 * also used for their identification in the petri-net, to a PolyTask. A
 * PolyTask may contain side-effects.
 *
 */
using Store = std::unordered_map<Transition, Callback>;

/**
 * @brief Forward declaration for the implementation of the PetriNet class.
 * This is used to hide implementation from the end-user and speed up
 * compilation times.
 *
 */
struct Petri;

}  // namespace symmetri

/**
 * @brief PetriNet exposes the possible constructors to create PetriNets.
 *
 */
class PetriNet final {
 public:
  /**
   * @brief Construct a new PetriNet object from a set of paths to pnml-files
   *
   * @param path_to_pnml
   * @param final_marking
   * @param store
   * @param priority
   * @param case_id
   * @param stp
   */
  PetriNet(const std::set<std::string> &path_to_pnml,
           const symmetri::Marking &final_marking, const symmetri::Store &store,
           const symmetri::PriorityTable &priority, const std::string &case_id,
           std::shared_ptr<symmetri::TaskSystem> stp);

  /**
   * @brief Construct a new PetriNet object from a set of paths to
   * grml-files. Grml fils already have priority, so they are not needed.
   *
   * @param path_to_grml
   * @param final_marking
   * @param store
   * @param case_id
   * @param stp
   */
  PetriNet(const std::set<std::string> &path_to_grml,
           const symmetri::Marking &final_marking, const symmetri::Store &store,
           const std::string &case_id,
           std::shared_ptr<symmetri::TaskSystem> stp);

  /**
   * @brief Construct a new PetriNet object from a net and initial marking
   *
   * @param net
   * @param m0
   * @param final_marking
   * @param store
   * @param priority
   * @param case_id
   * @param stp
   */
  PetriNet(const symmetri::Net &net, const symmetri::Marking &m0,
           const symmetri::Marking &final_marking, const symmetri::Store &store,
           const symmetri::PriorityTable &priority, const std::string &case_id,
           std::shared_ptr<symmetri::TaskSystem> stp);

  /**
   * @brief register transition gives a handle to manually force a transition to
   * fire. It returns a callable handle that will schedule a reducer for the
   * transition, generating tokens.
   *
   * @param transition the name of transition
   * @return std::function<void()>
   */
  std::function<void()> registerTransitionCallback(
      const std::string &transition) const noexcept;

  /**
   * @brief Get the Marking object. This function is thread-safe and be called
   * during PetriNet execution.
   *
   * @return std::vector<symmetri::Place>
   */
  std::vector<symmetri::Place> getMarking() const noexcept;

  /**
   * @brief reuseApplication resets the PetriNet such that the same net can
   * be used again after a cancel call or natural termination of the PetriNet.
   * You do need to supply a new case_id which must be different.
   *
   * @param case_id needs to be a new unique id
   * @return true
   * @return false
   */
  bool reuseApplication(const std::string &case_id);

  friend symmetri::State(::fire)(const PetriNet &);
  friend void(::cancel)(const PetriNet &);
  friend void(::pause)(const PetriNet &);
  friend void(::resume)(const PetriNet &);
  friend symmetri::Eventlog(::getLog)(const PetriNet &);

 private:
  const std::shared_ptr<symmetri::Petri>
      impl;  ///< Pointer to the implementation, all
             ///< information is stored in Petri
};
