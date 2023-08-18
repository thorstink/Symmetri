#pragma once
#include <blockingconcurrentqueue.h>

#include <functional>
#include <memory>
#include <optional>

#include "small_vector.hpp"
#include "symmetri/callback.h"
#include "symmetri/tasks.h"
#include "symmetri/types.h"

namespace symmetri {

using SmallVector = gch::small_vector<size_t, 4>;
using Store = std::unordered_map<Transition, Callback>;

/**
 * @brief a small helper function to get the index representation of a place or
 * transition.
 *
 * @param m
 * @param s
 * @return size_t
 */
size_t toIndex(const std::vector<std::string> &m, const std::string &s);

/**
 * @brief calculates a list of possible transitions given the current
 * token-distribution. It returns a list of transitions sorted by priority. The
 * first element being the high priority.
 *
 * @param tokens
 * @param p_to_ts_n
 * @param priorities
 * @return gch::small_vector<uint8_t, 32>
 */
gch::small_vector<uint8_t, 32> possibleTransitions(
    const std::vector<size_t> &tokens,
    const std::vector<SmallVector> &p_to_ts_n,
    const std::vector<int8_t> &priorities);

/**
 * @brief Takes a vector of input places (pre-conditions) and the current token
 * distribution to determine whether the pre-conditions are met.
 *
 * @param pre
 * @param tokens
 * @return true if the pre-conditions are met
 * @return false otherwise
 */
bool canFire(const SmallVector &pre, const std::vector<size_t> &tokens);

struct Petri;
using Reducer = std::function<void(Petri &)>;

/**
 * @brief Create a Reducer For Transition object
 *
 * @param T_i
 * @param task
 * @param case_id
 * @param reducers
 * @return Reducer
 */
Reducer fireTransition(
    size_t T_i, const std::string &transition, const Callback &task,
    const std::string &case_id,
    const std::shared_ptr<moodycamel::BlockingConcurrentQueue<Reducer>>
        &reducers);

/**
 * @brief Petri is a different data structure to encode the Petri net. It is
 * optimized for calculating the fireable transitions and quick lookups in
 * ordered vectors.
 *
 */
struct Petri {
  /**
   * @brief Construct a new Petri from a multiset description of a Petri
   * net, a lookup table for the transitions, optional priorities and an initial
   * marking. A lot of conversion work is done in the constructor, so you should
   * avoid creating Models during the run-time of a Petri application.
   *
   * @param net
   * @param store
   * @param priority
   * @param M0
   */
  explicit Petri(const Net &_net, const Store &_store,
                 const PriorityTable &_priority, const Marking &_initial_tokens,
                 const Marking &_final_marking, const std::string &_case_id,
                 std::shared_ptr<TaskSystem> stp);
  ~Petri() noexcept = default;
  Petri(Petri const &) = delete;
  Petri(Petri &&) noexcept = delete;
  Petri &operator=(Petri const &) = delete;
  Petri &operator=(Petri &&) noexcept = delete;

  /**
   * @brief outputs the marking as a vector of tokens; e.g. [1 1 1 0 5] means 3
   * tokens in place 1, 1 token in place 0 and 1 token in place 5.
   *
   * @param marking
   * @return std::vector<size_t>
   */
  std::vector<size_t> toTokens(const Marking &marking) const noexcept;

  /**
   * @brief Get the current marking. It is represented by a vector of places:
   * every occurance of a place in this vectors implies a token in that place.
   * This representation does not show the empty places explicitly; if a place
   * is not in this vector you can assume it has no token.
   *
   * @return std::vector<Place>
   */
  std::vector<Place> getMarking() const;

  /**
   * @brief get the current eventlog, also copies in all child eventlogs of
   * active petri nets.
   *
   * @return Eventlog
   */
  Eventlog getLog() const;

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
   * @brief Gives a list of unique transitions that are fireable given the
   * marking at the time of calling this function, sorted by priority.
   *
   * @return std::vector<Transition>
   */
  std::vector<Transition> getFireableTransitions() const;

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
  bool fire(const Transition &t,
            const std::shared_ptr<moodycamel::BlockingConcurrentQueue<Reducer>>
                &reducers,
            const std::string &case_id = "undefined_case_id");

  /**
   * @brief Tries to run a transition (or all) until it 'deadlocks'. The list
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
      bool run_all = true, const std::string &case_id = "undefined_case_id");

  struct {
    /**
     * @brief (ordered) list of string representation of transitions
     *
     */
    std::vector<Transition> transition;

    /**
     * @brief (ordered) list of string representation of places
     *
     */
    std::vector<Place> place;

    /**
     * @brief list of list of inputs to transitions. This vector is indexed like
     * `transition`.
     *
     */
    std::vector<SmallVector> input_n;

    /**
     * @brief list of list of outputs of transitions. This vector is indexed
     * like `transition`.
     *
     */
    std::vector<SmallVector> output_n;

    /**
     * @brief list of list of transitions that have places as inputs. This
     * vector is index like `place`
     *
     */
    std::vector<SmallVector> p_to_ts_n;

    /**
     * @brief This vector holds priorities for all transitions. This vector is
     * index like `transition`.
     *
     */
    std::vector<int8_t> priority;

    /**
     * @brief This is the same 'lookup table', only index using  `transition` so
     * it is compatible with index lookup.
     *
     */
    std::vector<Callback> store;
  } net;  ///< Is a data-oriented design of a Petri net

  std::vector<size_t> initial_tokens;      ///< The initial marking
  std::vector<size_t> tokens;              ///< The current marking
  std::vector<size_t> final_marking;       ///< The final marking
  std::vector<size_t> active_transitions;  ///< List of active transitions
  Eventlog event_log;                      ///< The most actual event_log
  State state;                             ///< The current state of the Petri
  std::string case_id;  ///< The unique identifier for this Petri-run
  std::atomic<std::optional<unsigned int>>
      thread_id_;  ///< The id of the thread from which the Petri is fired.

  std::shared_ptr<moodycamel::BlockingConcurrentQueue<Reducer>> reducer_queue;
  std::shared_ptr<TaskSystem>
      pool;  ///< A pointer to the threadpool used to defer Callbacks.

 private:
  /**
   * @brief fires a transition.
   *
   * @param t transition as index in transition vector
   * @param reducers
   * @param polymorphic_actions
   * @param case_id
   * @return true if the transition is direct.
   * @return false if it is only dispatched.
   */
  bool Fire(const size_t t,
            const std::shared_ptr<moodycamel::BlockingConcurrentQueue<Reducer>>
                &reducers,
            const std::string &case_id);
};

}  // namespace symmetri
