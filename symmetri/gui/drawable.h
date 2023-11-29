#pragma once

/** @file drawable.h */

#include <iostream>
#include <memory>

template <typename T>
void draw(T &) {
  std::cout << "joe" << std::endl;
}

class Drawable {
 private:
  struct concept_t {
    virtual ~concept_t() = default;
    virtual void draw_() = 0;
  };
  template <typename drawable_t>
  struct model final : concept_t {
    model(drawable_t &&x) : drawable_(std::move(x)) {}
    void draw_() override { return draw(drawable_); }
    drawable_t drawable_;
  };

 public:
  template <typename drawable_t>
  Drawable(drawable_t drawable)
      : self_(std::make_shared<model<drawable_t>>(std::move(drawable))) {}
  friend void draw(Drawable &drawable) { return drawable.self_->draw_(); }

  std::shared_ptr<concept_t> self_;
};
