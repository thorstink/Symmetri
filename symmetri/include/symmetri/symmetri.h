#pragma once

#include <functional>
#include <set>
#include <unordered_map>

#include "symmetri/polyaction.h"
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
 * @brief Forward declaration for the implementation of the Application class.
 * This is used to hide implementation from the end-user and speed up
 * compilation times.
 *
 */
struct Petri;

/**
 * @brief The Application class is a class that can create, configure and
 * run a Petri net.
 *
 */
class Application final {
 private:
  std::shared_ptr<Petri> impl;  ///< Pointer to the implementation, all
                                ///< information is stored in Petri
  std::function<void(const std::string &)>
      register_functor;  ///< At Application construction this function is
                         ///< created. It can be used to assign a trigger to
                         ///< transitions - allowing the user to invoke a
                         ///< transition without meeting the pre-conditions.

 public:
  /**
   * @brief Construct a new Application object from a set of paths to pnml-files
   *
   * @param path_to_pnml
   * @param final_marking
   * @param store
   * @param priority
   * @param case_id
   * @param stp
   */
  Application(const std::set<std::string> &path_to_pnml,
              const Marking &final_marking, const Store &store,
              const PriorityTable &priority, const std::string &case_id,
              std::shared_ptr<TaskSystem> stp);

  /**
   * @brief Construct a new Application object from a set of paths to
   * grml-files. Grml fils already have priority, so they are not needed.
   *
   * @param path_to_grml
   * @param final_marking
   * @param store
   * @param case_id
   * @param stp
   */
  Application(const std::set<std::string> &path_to_grml,
              const Marking &final_marking, const Store &store,
              const std::string &case_id, std::shared_ptr<TaskSystem> stp);

  /**
   * @brief Construct a new Application object from a net and initial marking
   *
   * @param net
   * @param m0
   * @param final_marking
   * @param store
   * @param priority
   * @param case_id
   * @param stp
   */
  Application(const Net &net, const Marking &m0, const Marking &final_marking,
              const Store &store, const PriorityTable &priority,
              const std::string &case_id, std::shared_ptr<TaskSystem> stp);

  /**
   * @brief This executes the net, like a transition, it returns a result.
   * This is equal to calling `fire(app)`.
   *
   * @return Result
   */
  Result run() const noexcept;

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
   * @brief Tries to fire the transition.
   *
   * @param transition
   * @return true if it fired the transition
   * @return false if the preconditions were not met, the transition was not
   * fired
   */
  bool tryFireTransition(const std::string &transition) const noexcept;

  /**
   * @brief Get the Event Log object. If the Petri net is running, this call is
   * blocking as it is executed on the Petri net execution loop. Otherwise it
   * directly returns the log.
   *
   *
   * @return Eventlog
   */
  Eventlog getEventLog() const noexcept;

  /**
   * @brief Get the State, represented by a vector of *active* transitions (who
   * can still produce reducers and hence marking mutations) and the *current
   * marking*. If the Petri net is running, this call is blocking as it is
   * executed on the Petri net execution loop. Otherwise it directly returns the
   * state.
   *
   * @return std::pair<std::vector<Transition>, std::vector<Place>>
   */
  std::pair<std::vector<Transition>, std::vector<Place>> getState()
      const noexcept;

  /**
   * @brief Get a vector of Fireable Transitions. If the Petri net is running,
   * this call is blocking as it is executed on the Petri net execution loop.
   * Otherwise it directly returns the vector of fireable transitions.
   *
   * @return std::vector<Transition>
   */
  std::vector<Transition> getFireableTransitions() const noexcept;

  /**
   * @brief cancel breaks the Petri net loop and recursively tries to cancel all
   * parent transitions
   *
   */
  Result cancel() const noexcept;

  /**
   * @brief reuseApplication resets the application such that the same net can
   * be used again after an cancel call. You do need to supply a new case_id
   * which must be different.
   *
   */
  bool reuseApplication(const std::string &case_id);

  /**
   * @brief pause prevents new transitions from being queued, even if their
   * marking requirement has been met. Reducers that still come in are
   * processed.
   *
   */
  void pause() const noexcept;

  /**
   * @brief resumes the petri net by allowing traditions to be fired again.
   *
   */
  void resume() const noexcept;
};

Result fire(const Application &app) { return app.run(); };

Result cancel(const Application &app) { return app.cancel(); }

bool isDirect(const Application &) { return false; };

void pause(const Application &app) { return app.pause(); };

void resume(const Application &app) { return app.resume(); };

}  // namespace symmetri
