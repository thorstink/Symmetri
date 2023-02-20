#pragma once
#define LLVM_SMALL_VECTOR_IMPLEMENTATION
#include <blockingconcurrentqueue.h>

#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <tuple>

#include "small_vector.hpp"
#include "symmetri/symmetri.h"
namespace symmetri {

using SmallVector = gch::small_vector<size_t, 4>;

size_t toIndex(const std::vector<std::string> &m, const std::string &s);

struct Model;
using Reducer = std::function<Model &(Model &&)>;

Reducer createReducerForTransition(size_t T_i, const PolyAction &task,
                                   const std::string &case_id);

/**
 * @brief Model is a different data structure to encode the Petri net. It is
 * optimized for calculating the fireable transitions and quick lookups in
 * ordered vectors.
 *
 */
struct Model {
  /**
   * @brief Construct a new Model from a multiset description of a Petri
   * net, a lookup table for the transitions, optional priorities and an initial
   * marking. A lot of conversion work is done in the constructor, so you should
   * avoid creating Models during the run-time of a Petri application.
   *
   * @param net
   * @param store
   * @param priority
   * @param M0
   */
  Model(const Net &net, const Store &store,
        const std::vector<std::pair<symmetri::Transition, int8_t>> &priority,
        const Marking &M0);

  /**
   * @brief Get the current marking. It is represented by a vector of places.
   * Every occurance of a place in this vectors implies a token in that place.
   * This representation does not show the empty places explicitly; if a place
   * is not in this vector you can assume it has no token.
   *
   * @return std::vector<Place>
   */
  std::vector<Place> getMarking() const;

  /**
   * @brief Get a vector of transitions that are *active*. Active means that
   * they are either in the transition queue or its transition has been fired.
   * In all cases in the future it will produce a reducer which will be
   * processed if the Petri net is not halted before the reducer is pop'd from
   * the reducer queue.
   *
   * @return std::vector<Transition>
   */
  std::vector<Transition> getActiveTransitions() const;

  /**
   * @brief Gives a list of transitions that are fireable given the marking at
   * the time of calling this function.
   *
   * @return std::vector<Transition>
   */
  std::vector<Transition> getFireableTransitions() const;

  /**
   * @brief Get the State; this is a pair of active transitions *and* the
   * current marking.
   *
   * @return std::pair<std::vector<Transition>, std::vector<Place>>
   */
  std::pair<std::vector<Transition>, std::vector<Place>> getState() const;

  /**
   * @brief Try to fire a single transition.
   *
   * @param t string representation of the transition
   * @param reducers pointer reducer queue, needed to construct a reducer
   * @param polymorphic_actions pointer to the threadpool, needed to dispatch
   * the transition payload
   * @param case_id the case id of the Petri instance
   * @return true If the transition was fireable, it was queued
   * @return false If the transition was not fireable, nothing happenend.
   */
  bool fireTransition(
      const Transition &t,
      const std::shared_ptr<moodycamel::BlockingConcurrentQueue<Reducer>>
          &reducers,
      const StoppablePool &polymorphic_actions,
      const std::string &case_id = "undefined_case_id");

  /**
   * @brief Tries to run a transition (or all) untill it 'deadlocks'. The list
   * of actions is queued and the function returns.
   *
   * @param reducers pointer reducer queue, needed to construct the reducer(s)
   * @param polymorphic_actions pointer to the threadpool, needed to dispatch
   * the transition payload(s)
   * @param run_all if true, potentially queues multiple transitions
   * @param case_id the case id of the Petri instance
   */
  void fireTransitions(
      const std::shared_ptr<moodycamel::BlockingConcurrentQueue<Reducer>>
          &reducers,
      const StoppablePool &polymorphic_actions, bool run_all = true,
      const std::string &case_id = "undefined_case_id");

  Model &operator=(const Model &) { return *this; }
  Model(const Model &) = delete;

  struct {
    std::vector<Transition> transition;
    std::vector<Place> place;
    std::vector<SmallVector> input_n, output_n, p_to_ts_n;
    std::vector<int8_t> priority;
    std::vector<PolyAction> store;
  } net;

  std::vector<size_t> initial_tokens;        ///< The intial marking
  std::vector<size_t> tokens_n;              ///< The current marking
  std::vector<size_t> active_transitions_n;  ///< List of active transitions
  clock_s::time_point timestamp;  ///< Timestamp of latest marking mutation
  Eventlog event_log;             ///< The most actual event_log

 private:
  bool tryFire(
      const size_t t,
      const std::shared_ptr<moodycamel::BlockingConcurrentQueue<Reducer>>
          &reducers,
      const StoppablePool &polymorphic_actions, const std::string &case_id);
};

}  // namespace symmetri
