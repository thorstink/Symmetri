#pragma once

#include <optional>
#include <set>
#include <string>

#include "Symmetri/types.h"

namespace symmetri {

struct Application {
 private:
  std::function<void(const std::string &t)> p;
  std::function<symmetri::OptionalError()> run;

 public:
  Application(const std::set<std::string> &path_to_petri,
              const TransitionActionMap &,
              const std::string &case_id = "NOCASE", bool use_webserver = true);
  symmetri::OptionalError operator()() const;

  template <typename T>
  inline std::function<void(T)> push(const symmetri::Transition &t) const {
    return [t, this](T) { p(t); };
  }
};

}  // namespace symmetri
