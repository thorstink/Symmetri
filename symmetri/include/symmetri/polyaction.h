#pragma once
#include <memory>

#include "symmetri/types.h"
namespace symmetri {

class PolyAction {
 public:
  template <typename T>
  PolyAction(T x) : self_(std::make_shared<model<T>>(std::move(x))) {}

  friend TransitionResult runTransition(const PolyAction &x) {
    return x.self_->run_();
  }

 private:
  struct concept_t {
    virtual ~concept_t() = default;
    virtual TransitionResult run_() const = 0;
  };
  template <typename T>
  struct model final : concept_t {
    model(T x) : transition_function_(std::move(x)) {}
    TransitionResult run_() const override {
      return runTransition(transition_function_);
    }
    T transition_function_;
  };

  std::shared_ptr<const concept_t> self_;
};
}  // namespace symmetri
