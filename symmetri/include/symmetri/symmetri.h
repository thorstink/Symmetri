#pragma once

#include <atomic>
#include <functional>
#include <set>
#include <unordered_map>

#include "symmetri/polytransition.h"
#include "symmetri/tasks.h"
#include "symmetri/types.h"

namespace symmetri {
/**
 * @brief A Store is a mapping from Transitions, represented by a string that is
 * also used for their identification in the petri-net, to a PolyTask. A
 * PolyTask may contain side-effects.
 *
 */
using Store = std::unordered_map<Transition, PolyTransition>;

/**
 * @brief Forward declaration for the implementation of the PetriNet class.
 * This is used to hide implementation from the end-user and speed up
 * compilation times.
 *
 */
struct Petri;

}  // namespace symmetri

/**
 * @brief The PetriNet class is a class that can create, configure and
 * run a Petri net.
 *
 */
class PetriNet;
symmetri::Result fire(const PetriNet &);
symmetri::Result cancel(const PetriNet &);
void pause(const PetriNet &);
void resume(const PetriNet &);
symmetri::Eventlog getLog(const PetriNet &);

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
   * fire. This is usefull if you want to trigger a transition that has no input
   * places. It is not recommended to use this for transitions with input
   * places! This violates the mathematics of petri nets.
   *
   * @param transition the name of transition. This transition has to be
   * available in the net.
   * @return std::function<void()>
   */
  std::function<void()> registerTransitionCallback(
      const std::string &transition) const noexcept;

  /**
   * @brief reuseApplication resets the application such that the same net can
   * be used again after an cancel call. You do need to supply a new case_id
   * which must be different.
   *
   */
  bool reuseApplication(const std::string &case_id);

  friend symmetri::Result(::fire)(const PetriNet &);
  friend symmetri::Result(::cancel)(const PetriNet &);
  friend void(::pause)(const PetriNet &);
  friend void(::resume)(const PetriNet &);
  friend symmetri::Eventlog(::getLog)(const PetriNet &);

 private:
  std::shared_ptr<symmetri::Petri>
      impl;  ///< Pointer to the implementation, all
             ///< information is stored in Petri
  std::function<void(const std::string &)>
      register_functor;  ///< At PetriNet construction this function is
                         ///< created. It can be used to assign a trigger to
                         ///< transitions - allowing the user to invoke a
                         ///< transition without meeting the pre-conditions.
};
