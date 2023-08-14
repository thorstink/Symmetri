#pragma once

#include <memory>

#include "symmetri/types.h"

struct DirectMutation {};

bool isDirect(const DirectMutation &);
symmetri::Result fire(const DirectMutation &);

/**
 * @brief Checks if the transition-function can be invoked.
 *
 * @tparam T The type of the transition.
 * @return true The pre- and post-marking-mutation can happen instantly.
 * @return false The pre-marking mutation happens instantly and the
 * post-marking-mutation should only happen after the transition is invoked.
 */
template <typename T>
bool isDirect(const T &) {
  return false;
}

/**
 * @brief The default cancel implementation is naive. It only returns a
 * user-exit state and does nothing to the actual transition itself, and it will
 * still complete. Its' reducer however is never processed.
 *
 * @tparam T
 * @return Result
 */
template <typename T>
symmetri::Result cancel(const T &) {
  return {{}, symmetri::State::UserExit};
}

/**
 * @brief Implments a pause action for a transition. By default it does nothing.
 *
 * @tparam T
 */
template <typename T>
void pause(const T &) {}

/**
 * @brief Templates a resume action for a transition. By default it does
 * nothing.
 *
 * @tparam T
 */
template <typename T>
void resume(const T &) {}

/**
 * @brief Generates a Result based on what kind of information the
 * transition-function returns.
 *
 * @tparam T The transition-function type.
 * @param transition The function to be executed.
 * @return Result Contains information on the result-state and
 * possible eventlog of the transition.
 */
template <typename T>
symmetri::Result fire(const T &transition) {
  if constexpr (std::is_same_v<DirectMutation, T>) {
    return {{}, symmetri::State::Completed};
  } else if constexpr (std::is_same_v<symmetri::State,
                                      decltype(transition())>) {
    return {{}, transition()};
  } else if constexpr (std::is_same_v<symmetri::Result,
                                      decltype(transition())>) {
    return transition();
  } else {
    transition();
    return {{}, symmetri::State::Completed};
  }
}

template <typename T>
symmetri::Eventlog getLog(const T &) {
  return {};
}

namespace symmetri {

/**
 * @brief PolyTransition is a wrapper around any type that you want to tie to a
 * transition. Typically this is an invokable object, such as a function, that
 * executes some side-effects. The output of the invokable object can be used to
 * communicate success or failure to the petri-net executor. You can create
 * custom behavior by defining a tailored "fire(const A&)" for your
 * transition payload.
 *
 * Random note, this class is inspired/taken by Sean Parents' talk "inheritance
 * is the base class of evil".
 *
 */
class PolyTransition {
 public:
  template <typename Transition>
  PolyTransition(Transition x)
      : self_(std::make_shared<model<Transition>>(std::move(x))) {}
  friend Result fire(const PolyTransition &x) { return x.self_->fire_(); }
  friend Eventlog getLog(const PolyTransition &x) {
    return x.self_->get_log_();
  }
  friend bool isDirect(const PolyTransition &x) {
    return x.self_->is_direct_();
  }
  friend Result cancel(const PolyTransition &x) { return x.self_->cancel_(); }
  friend void pause(const PolyTransition &x) { return x.self_->pause_(); }
  friend void resume(const PolyTransition &x) { return x.self_->resume_(); }

 private:
  struct concept_t {
    virtual ~concept_t() = default;
    virtual Result fire_() const = 0;
    virtual Eventlog get_log_() const = 0;
    virtual Result cancel_() const = 0;
    virtual void pause_() const = 0;
    virtual void resume_() const = 0;
    virtual bool is_direct_() const = 0;
  };

  /**
   * @brief A transition is defined by the concept that it is runnable,
   * cancellable, pauseable and resumable. It can also be direct, meaning there
   * is no associated side-effects but it is only a instant marking mutation.
   *
   * @tparam T the actual transition contain business logic
   */
  template <typename Transition>
  struct model final : concept_t {
    model(Transition &&x) : transition_(std::move(x)) {}
    Result fire_() const override { return fire(transition_); }
    Eventlog get_log_() const override { return getLog(transition_); }
    Result cancel_() const override { return cancel(transition_); }
    bool is_direct_() const override { return isDirect(transition_); }
    void pause_() const override { return pause(transition_); }
    void resume_() const override { return resume(transition_); }
    Transition transition_;
  };

  std::shared_ptr<const concept_t> self_;
};

}  // namespace symmetri
