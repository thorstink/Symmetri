#pragma once

/** @file symmetri.h */

#include <set>

#include "symmetri/callback.h"
#include "symmetri/tasks.h"
#include "symmetri/types.h"

namespace symmetri {

/**
 * @brief Forward declaration for the implementation of the PetriNet class.
 * This is used to hide implementation from the end-user and speed up
 * compilation times.
 *
 */
struct Petri;

/**
 * @brief PetriNet exposes the possible constructors to create PetriNets.
 *
 */
class PetriNet final {
 public:
  /**
   * @brief Construct a new PetriNet object from a set of paths to PNML- or
   * GRML-files. Since PNML-files do not have priorities; you can optionally add
   * a priority table manually.
   *
   * @param petri_net_xmls
   * @param case_id
   * @param threadpool
   * @param goal_marking
   * @param priorities
   */
  PetriNet(const std::set<std::string> &petri_net_xmls,
           const std::string &case_id,
           std::shared_ptr<symmetri::TaskSystem> threadpool,
           const symmetri::Marking &goal_marking = {},
           const symmetri::PriorityTable &priorities = {});

  /**
   * @brief Construct a new PetriNet object from a net and initial marking
   *
   * @param net
   * @param case_id
   * @param threadpool
   * @param initial_marking
   * @param goal_marking
   * @param priorities
   */
  PetriNet(const symmetri::Net &net, const std::string &case_id,
           std::shared_ptr<symmetri::TaskSystem> threadpool,
           const symmetri::Marking &initial_marking,
           const symmetri::Marking &goal_marking = {},
           const symmetri::PriorityTable &priorities = {});

  /**
   * @brief By registering a input transition you get a handle to manually force
   * a transition to fire. It returns a callable handle that will schedule a
   * reducer for the transition, generating tokens. This only works for
   * transitions that have no input places.
   *
   * @param transition the name of transition
   * @return std::function<void()>
   */
  std::function<void()> getInputTransitionHandle(
      const std::string &transition) const noexcept;

  /**
   * @brief The default transition payload (DirectMutation) is overload by the
   * Callback supplied for a specific transition.
   *
   * @param transition the name of transition
   * @param callback the callback
   */
  void registerCallback(const std::string &transition,
                        const symmetri::Callback &callback) const noexcept;

  /**
   * @brief Get the Marking object. This function is thread-safe and be called
   * during PetriNet execution.
   *
   * @return std::vector<symmetri::Place>
   */
  symmetri::Marking getMarking() const noexcept;

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

  friend symmetri::Token(::fire)(const PetriNet &);
  friend void(::cancel)(const PetriNet &);
  friend void(::pause)(const PetriNet &);
  friend void(::resume)(const PetriNet &);
  friend symmetri::Eventlog(::getLog)(const PetriNet &);

 private:
  const std::shared_ptr<symmetri::Petri>
      impl;  ///< Pointer to the implementation, all
             ///< information is stored in Petri
};

}  // namespace symmetri
