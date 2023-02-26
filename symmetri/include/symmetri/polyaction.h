#pragma once
#include <memory>

#include "symmetri/types.h"
namespace symmetri {

/**
 * @brief PolyAction is a wrapper around any type that you want to tie to a
 * transition. Typically this is an invokable object, such as a function, that
 * executes some side-effects. The output of the invokable object can be used to
 * communicate success or failure to the petri-net executor. You can create
 * custom behavior by defining a tailored "fireTransition(const A&)" for your
 * transition payload.
 *
 * Random note, this class is inspired/taken by Sean Parents' talk "inheritance
 * is the base class of evil".
 *
 */
class PolyAction {
 public:
  template <typename T>
  PolyAction(T x) : self_(std::make_shared<model<T>>(std::move(x))) {}

  /**
   * @brief You can define your own specialized fireTransition function for your
   * transition type. This allows you to control what should happen when your
   * transition is fired. If you do not define it, Symmetri will try to invoke
   * it, and otherwise handle it as direct transition.
   *
   * @param x
   * @return Result
   */
  friend Result fireTransition(const PolyAction &x) { return x.self_->run_(); }

  /**
   * @brief You can define a transition payload to be simple by creating a
   * specialized isDirectTransition for your type.
   *
   * @param x
   * @return true
   * @return false
   */
  friend bool isDirectTransition(const PolyAction &x) {
    return x.self_->simple_();
  }

  /**
   * @brief
   *
   * @param x
   * @return Result
   */
  friend Result cancelTransition(const PolyAction &x) {
    return x.self_->cancel_();
  }

 private:
  struct concept_t {
    virtual ~concept_t() = default;
    virtual Result run_() const = 0;
    virtual Result cancel_() const = 0;
    virtual bool simple_() const = 0;
  };
  template <typename T>
  struct model final : concept_t {
    model(T x) : payload_(std::move(x)) {}
    Result run_() const override { return fireTransition(payload_); }
    Result cancel_() const override { return cancelTransition(payload_); }
    bool simple_() const override { return isDirectTransition(payload_); }
    T payload_;
  };

  std::shared_ptr<const concept_t> self_;
};
}  // namespace symmetri
