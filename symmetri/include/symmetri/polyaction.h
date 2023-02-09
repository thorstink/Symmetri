#pragma once
#include <memory>

#include "symmetri/types.h"
namespace symmetri {

/**
 * @brief PolyAction is a wrapper around any type that you want to tie to a
 * transition. Typically this is an invokable object, such as a function, that
 * executes some side-effects. The output of the invokable object can be used to
 * communicate success or failure to the petri-net executor.
 *
 */
class PolyAction {
 public:
  template <typename T>
  PolyAction(T x) : self_(std::make_shared<model<T>>(std::move(x))) {}

  friend Result runTransition(const PolyAction &x) { return x.self_->run_(); }
  friend bool isDirectTransition(const PolyAction &x) {
    return x.self_->simple_();
  }

 private:
  struct concept_t {
    virtual ~concept_t() = default;
    virtual Result run_() const = 0;
    virtual bool simple_() const = 0;
  };
  template <typename T>
  struct model final : concept_t {
    model(T x) : transition_function_(std::move(x)) {}
    Result run_() const override { return runTransition(transition_function_); }
    bool simple_() const override {
      return isDirectTransition(transition_function_);
    }
    T transition_function_;
  };

  std::shared_ptr<const concept_t> self_;
};
}  // namespace symmetri
