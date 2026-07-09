#pragma once

/** @file callback.h */

#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "symmetri/types.h"

namespace symmetri {

/**
 * @brief The full result of firing a transition. `state` is what the event
 * log records. If `deposits` is set, exactly those tokens are placed (each
 * must name an output place of the transition; omitting an output place is
 * how a callback branches). If it is not set, the legacy behavior applies:
 * a token colored `state` is deposited in every output place.
 */
struct FireResult {
  Token state;
  std::optional<Marking> deposits;
};

#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace detail {

/**
 * @brief Compile-time signature inspection for typed callbacks: function
 * pointers and (non-generic) functors/lambdas via their call operator.
 */
template <typename T, typename = void>
struct function_traits;  // primary: not a typed callable

template <typename R, typename... Args>
struct function_traits<R (*)(Args...)> {
  static constexpr size_t arity = sizeof...(Args);
  using args_t = std::tuple<std::decay_t<Args>...>;
};

template <typename R, typename... Args>
struct function_traits<R (*)(Args...) noexcept>
    : function_traits<R (*)(Args...)> {};

template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...)> : function_traits<R (*)(Args...)> {};

template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...) const>
    : function_traits<R (*)(Args...)> {};

template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...) const noexcept>
    : function_traits<R (*)(Args...)> {};

template <typename T>
struct function_traits<T, std::void_t<decltype(&T::operator())>>
    : function_traits<decltype(&T::operator())> {};

/**
 * @brief A callable whose exact argument types are deducible (so consumed
 * tokens' payloads can be unpacked into them). Generic lambdas and overloaded
 * call operators are not — those can take the raw token vector instead.
 */
template <typename T>
concept TypedCallable = requires { typename function_traits<T>::args_t; };

/**
 * @brief Invoke a thunk and normalize its result into a FireResult:
 * void -> Success, Token-convertible -> that color (legacy: stamped on every
 * output place), Marking -> Success + exactly those deposits, FireResult ->
 * passed through.
 */
template <typename Thunk>
FireResult normalize(Thunk &&thunk) {
  using R = std::invoke_result_t<Thunk>;
  if constexpr (std::is_void_v<R>) {
    thunk();
    return {Success, std::nullopt};
  } else if constexpr (std::is_same_v<std::decay_t<R>, FireResult>) {
    return thunk();
  } else if constexpr (std::is_same_v<std::decay_t<R>, Marking>) {
    return {Success, thunk()};
  } else {
    return {Token(thunk()), std::nullopt};
  }
}

/**
 * @brief Call a typed callback by unpacking the consumed tokens' payloads
 * into its parameters. Matching is color-driven: each parameter of type A is
 * fed by the first unclaimed consumed token whose color is bound to A (the
 * type defines the token). Dataless tokens (e.g. a Success guard token) are
 * simply skipped; duplicate same-colored tokens resolve in input-arc order.
 * A parameter with no matching consumed token fails the firing.
 */
template <typename T, size_t... I>
FireResult invokeUnpacked(const T &callback,
                          const std::vector<AugmentedToken> &consumed,
                          std::index_sequence<I...>) {
  using args_t = typename function_traits<T>::args_t;
  constexpr size_t arity = sizeof...(I);
  const std::array<const std::type_info *, arity> wanted = {
      &typeid(std::tuple_element_t<I, args_t>)...};
  std::array<size_t, arity> pick = {};
  std::array<bool, 64> claimed = {};  // consumed.size() is arc-count bounded
  for (size_t k = 0; k < arity; k++) {
    bool found = false;
    for (size_t j = 0; j < consumed.size() && j < claimed.size(); j++) {
      const std::type_info *bound = consumed[j].color.boundType();
      if (!claimed[j] && bound != nullptr && *bound == *wanted[k]) {
        pick[k] = j;
        claimed[j] = true;
        found = true;
        break;
      }
    }
    if (!found) {
      return {Failed, std::nullopt};
    }
  }
  return normalize([&] {
    return callback(
        consumed[pick[I]].template get<std::tuple_element_t<I, args_t>>()...);
  });
}

}  // namespace detail
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

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
 * produce a reducer which potentially can still be processed.
 *
 * @tparam T the type of the callback.
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
 * @brief Generates a Token based on what kind of information the
 * Callback returns.
 *
 * @tparam T the type of the callback.
 * @param callback The function to be executed.
 * @return Token Contains information on the result-state and
 * possible eventlog of the callback.
 */
template <typename T>
Token fire(const T &callback) {
  if constexpr (std::is_same_v<Token, decltype(callback())>) {
    return callback();
  } else if constexpr (std::is_convertible_v<decltype(callback()), Token>) {
    return Token(callback());
  } else if constexpr (std::is_same_v<void, decltype(callback())>) {
    callback();
    return Success;
  }
}

/**
 * @brief Fire a callback with access to the tokens the transition consumed.
 * The callback's shape is picked at compile time:
 *
 *  1. `f(const std::vector<AugmentedToken>&)` — receives the consumed tokens
 *     (payloads included) in input-arc order.
 *  2. `f()` — legacy nullary callback; consumed tokens are dropped.
 *  3. `f(const A&, const B&, ...)` — typed parameters; the consumed tokens'
 *     payloads are unpacked into them positionally (input-arc order).
 *  4. anything else — falls back to the `Token fire(const T&)` customization
 *     point (e.g. PetriNet, DirectMutation, user-specialized types).
 *
 * Every shape may return void, Token, Marking (a set of deposits: exactly
 * those tokens are placed, on a subset of the output places) or FireResult.
 * A payload type mismatch (std::bad_any_cast) yields Failed.
 */
template <typename T>
FireResult fire(const T &callback,
                const std::vector<AugmentedToken> &consumed) {
  try {
    if constexpr (std::is_invocable_v<T, const std::vector<AugmentedToken> &>) {
      return detail::normalize([&] { return callback(consumed); });
    } else if constexpr (std::is_invocable_v<T>) {
      return detail::normalize([&] { return callback(); });
    } else if constexpr (detail::TypedCallable<T>) {
      return detail::invokeUnpacked(
          callback, consumed,
          std::make_index_sequence<detail::function_traits<T>::arity>{});
    } else {
      return {fire(callback), std::nullopt};
    }
  } catch (const std::bad_any_cast &) {
    return {Failed, std::nullopt};
  } catch (const TokenTypeError &) {
    return {Failed, std::nullopt};
  }
}

/**
 * @brief Get the Log object. By default it returns an empty vector.
 *
 * @tparam T the type of the callback.
 * @return Eventlog
 */
template <typename T>
Eventlog getLog(const T &) {
  return {};
}

template <typename T>
struct identity {
  typedef T type;
};

/**
 * @brief Callback is a wrapper around any type that you to tie to a
 * transition. Typically this is an invokable object, such as a function, that
 * executes some side-effects, but it can by anything if you implement a
 * fire function for it. The output can be used to communicate success or
 * failure to the petri-net executor. You can create custom behavior by defining
 * a tailored "Token fire(const A&)" for your class A.
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
   * @param callback is the Callback instance
   */
  Callback(Transition callback)
      : self_(std::make_shared<model<Transition>>(std::move(callback))) {}

  template <typename T, typename... Args>
  Callback(identity<T>, Args &&...args)
      : self_(std::make_shared<model<T>>(std::forward<Args>(args)...)) {}

  Callback(Callback &&f) = default;
  Callback &operator=(Callback &&other) = default;
  Callback(const Callback &o) = delete;
  Callback &operator=(const Callback &other) = delete;

  friend FireResult fire(const Callback &callback,
                         const std::vector<AugmentedToken> &consumed) {
    return callback.self_->fire_(consumed);
  }
  /// Legacy single-token fire; equivalent to firing with nothing consumed.
  friend Token fire(const Callback &callback) {
    return callback.self_->fire_({}).state;
  }
  friend Eventlog getLog(const Callback &callback) {
    return callback.self_->get_log_();
  }
  friend bool isSynchronous(const Callback &callback) {
    return callback.self_->is_synchronous_();
  }
  friend void cancel(const Callback &callback) {
    return callback.self_->cancel_();
  }
  friend void pause(const Callback &callback) {
    return callback.self_->pause_();
  }
  friend void resume(const Callback &callback) {
    return callback.self_->resume_();
  }
  auto getEndTime() const {
    return self_->end_t_.load(std::memory_order_relaxed);
  }

 private:
  struct concept_t {
    virtual ~concept_t() = default;
    virtual FireResult fire_(
        const std::vector<AugmentedToken> &consumed) const = 0;
    virtual Eventlog get_log_() const = 0;
    virtual void cancel_() const = 0;
    virtual void pause_() const = 0;
    virtual void resume_() const = 0;
    virtual bool is_synchronous_() const = 0;
    mutable std::atomic<Clock::time_point> end_t_{Clock::time_point::min()};
  };

  /**
   * @brief A transition is defined by the concept that it is runnable,
   * cancellable, pauseable and resumable. It can also be synchronous or
   * asynchronous and queried for logs.
   *
   * @tparam T the actual Callback contain business logic
   */
  template <typename Transition>
  struct model final : concept_t {
    explicit model(Transition &&t) : transition_(std::move(t)) {}
    template <typename... Args>
    model(Args &&...args) : transition_(std::forward<Args>(args)...) {}

    FireResult fire_(
        const std::vector<AugmentedToken> &consumed) const override {
      auto res = fire(transition_, consumed);
      end_t_.store(Clock::now(), std::memory_order_relaxed);
      return res;
    }
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
