#pragma once

/** @file callback.h */

#include <memory>

#include "symmetri/types.h"

/**
 * @brief A DirectMutation is a synchronous Callback that always
 * completes.
 *
 */
struct DirectMutation {};
bool isSynchronous(const DirectMutation &);
symmetri::State fire(const DirectMutation &);

/**
 * @brief Checks if the callback is synchronous. Synchronous callbacks are
 * immediately executed inside the Petri net executor. Asynchronous callbacks
 * are deferred to the TaskSystem.
 *
 * @tparam T the type of the callback.
 * @return true the callback will be executed synchronously.
 * @return false the callback will be executed asynchronously
 */
template <typename T>
bool isSynchronous(const T &) {
  return false;
}

/**
 * @brief The default cancel implementation is naive. It only returns a
 * user-exit state and does nothing to the actual Callback itself, and it will
 * still complete. Because the transition is by default not cancelled, it will
 * produce a reducer, however, it will never be processed.
 *
 * @tparam T the type of the callback.
 * @return Result, can be used to return the state at the moment on
 * cancellation.
 */
template <typename T>
void cancel(const T &) {}

/**
 * @brief Implements a pause action for a Callback. By default it does nothing
 * and not pause the Callback.
 *
 * @tparam T the type of the callback.
 */
template <typename T>
void pause(const T &) {}

/**
 * @brief Templates a resume action for a Callback.  By default it does nothing
 * and not resume the Callback.
 *
 * @tparam T the type of the callback.
 */
template <typename T>
void resume(const T &) {}

/**
 * @brief Generates a Result based on what kind of information the
 * Callback returns.
 *
 * @tparam T the type of the callback.
 * @param callback The function to be executed.
 * @return Result Contains information on the result-state and
 * possible eventlog of the callback.
 */
template <typename T>
symmetri::State fire(const T &callback) {
  if constexpr (std::is_same_v<symmetri::State, decltype(callback())>) {
    return callback();
  } else if constexpr (std::is_same_v<void, decltype(callback())>) {
    callback();
    return symmetri::State::Completed;
  }
}

/**
 * @brief Get the Log object. By default it returns an empty list.
 *
 * @tparam T the type of the callback.
 * @return symmetri::Eventlog
 */
template <typename T>
symmetri::Eventlog getLog(const T &) {
  return {};
}

namespace symmetri {

/**
 * @brief Callback is a wrapper around any type that you to tie to a
 * transition. Typically this is an invokable object, such as a function, that
 * executes some side-effects, but it can by anything if you implement a
 * fire-function for it. The output can be used to communicate success or
 * failure to the petri-net executor. You can create custom behavior by defining
 * a tailored "State fire(const A&)" for your class A.
 *
 * Random note, this class is inspired/taken by Sean Parents' talk "inheritance
 * is the base class of evil".
 *
 */
class Callback {
 public:
  template <typename Transition>
  /**
   * @brief Construct a new Callback object
   *
   * @param cb is the Callback instance
   */
  Callback(Transition cb)
      : self_(std::make_shared<model<Transition>>(std::move(cb))) {}

  State operator()() const { return this->self_->fire_(); }
  void operator()(Eventlog &el) const { el = this->self_->get_log_(); }
  friend State fire(const Callback &cb) { return cb.self_->fire_(); }
  friend Eventlog getLog(const Callback &cb) { return cb.self_->get_log_(); }
  friend bool isSynchronous(const Callback &cb) {
    return cb.self_->is_synchronous_();
  }
  friend void cancel(const Callback &cb) { return cb.self_->cancel_(); }
  friend void pause(const Callback &cb) { return cb.self_->pause_(); }
  friend void resume(const Callback &cb) { return cb.self_->resume_(); }

 private:
  struct concept_t {
    virtual ~concept_t() = default;
    virtual State fire_() const = 0;
    virtual Eventlog get_log_() const = 0;
    virtual void cancel_() const = 0;
    virtual void pause_() const = 0;
    virtual void resume_() const = 0;
    virtual bool is_synchronous_() const = 0;
  };

  /**
   * @brief A transition is defined by the concept that it is runnable,
   * cancellable, pauseable and resumable. It can also be synchronous or
   * asynchronous.
   *
   * @tparam T the actual Callback contain business logic
   */
  template <typename Transition>
  struct model final : concept_t {
    model(Transition &&x) : transition_(std::move(x)) {}
    State fire_() const override { return fire(transition_); }
    Eventlog get_log_() const override { return getLog(transition_); }
    void cancel_() const override { return cancel(transition_); }
    bool is_synchronous_() const override { return isSynchronous(transition_); }
    void pause_() const override { return pause(transition_); }
    void resume_() const override { return resume(transition_); }
    Transition transition_;
  };

  std::shared_ptr<const concept_t> self_;
};

}  // namespace symmetri
