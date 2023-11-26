#pragma once

/** @file drawable.h */

#include <iostream>
#include <memory>

class Drawable {
 public:
  template <typename drawable_t>
  Drawable(drawable_t drawable)
      : self_(std::make_shared<model<drawable_t>>(std::move(drawable))) {}
  friend void draw(const Drawable &drawable) { return drawable.self_->draw_(); }

 private:
  struct concept_t {
    virtual ~concept_t() = default;
    virtual void draw_() const = 0;
  };
  template <typename drawable_t>
  struct model final : concept_t {
    model(drawable_t &&x) : drawable_(std::move(x)) {}
    void draw_() const override { return draw(drawable_); }
    drawable_t drawable_;
  };

  std::shared_ptr<const concept_t> self_;
};
