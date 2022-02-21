#pragma once

#include <optional>
#include <set>
#include <string>

#include "Symmetri/types.h"

namespace symmetri {

struct input {
 private:
  std::optional<std::function<void(const std::string &t)>> p;

  friend void registerQueue(input &input,
                            std::function<void(const std::string &)> f);

 public:
  template <typename T>
  std::function<void(T)> push(const symmetri::Transition &t) {
    return [t, this](T) {
      if (p.has_value()) {
        p.value()(t);
      }
    };
  }
};

std::function<symmetri::OptionalError()> start(
    const std::set<std::string> &path_to_petri, const TransitionActionMap &,
    input &input, const std::string &case_id = "NOCASE", bool interface = true);
}  // namespace symmetri
