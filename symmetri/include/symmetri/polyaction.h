#pragma once
#include <memory>

#include "symmetri/types.h"
namespace symmetri {

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
  return !std::is_invocable_v<T>;
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
Result cancel(const T &) {
  return {{}, State::UserExit};
}

template <typename T>
void pause(const T &) {}

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
Result fire(const T &transition) {
  if constexpr (!std::is_invocable_v<T>) {
    return {{}, State::Completed};
  } else if constexpr (std::is_same_v<State, decltype(transition())>) {
    return {{}, transition()};
  } else if constexpr (std::is_same_v<Result, decltype(transition())>) {
    return transition();
  } else {
    transition();
    return {{}, State::Completed};
  }
}

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
  template <typename T>
  PolyTransition(T x) : self_(std::make_shared<model<T>>(std::move(x))) {}

  /**
   * @brief You can define your own specialized fire function for your
   * transition type. This allows you to control what should happen when your
   * transition is fired. If you do not define it, Symmetri will try to invoke
   * it, and otherwise handle it as direct transition.
   *
   * @param x
   * @return Result
   */
  friend Result fire(const PolyTransition &x) { return x.self_->run_(); }

  /**
   * @brief You can define a transition payload to be simple by creating a
   * specialized isDirect for your type.
   *
   * @param x
   * @return true
   * @return false
   */
  friend bool isDirect(const PolyTransition &x) {
    return x.self_->is_direct_();
  }

  /**
   * @brief
   *
   * @param x
   * @return Result
   */
  friend Result cancel(const PolyTransition &x) { return x.self_->cancel_(); }
  friend void pause(const PolyTransition &x) { return x.self_->pause_(); }
  friend void resume(const PolyTransition &x) { return x.self_->resume_(); }

 private:
  struct concept_t {
    virtual ~concept_t() = default;
    virtual Result run_() const = 0;
    virtual Result cancel_() const = 0;
    virtual void pause_() const = 0;
    virtual void resume_() const = 0;
    virtual bool is_direct_() const = 0;
  };
  template <typename T>
  struct model final : concept_t {
    model(T x) : transition_(std::move(x)) {}
    Result run_() const override { return fire(transition_); }
    Result cancel_() const override { return cancel(transition_); }
    bool is_direct_() const override { return isDirect(transition_); }
    void pause_() const override { return pause(transition_); }
    void resume_() const override { return resume(transition_); }
    T transition_;
  };

  std::shared_ptr<const concept_t> self_;
};

}  // namespace symmetri
