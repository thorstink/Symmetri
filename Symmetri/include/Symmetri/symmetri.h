#pragma once

#include <functional>
#include <set>
#include <string>
#include <unordered_map>
#include <variant>

#include "../../types.h"
namespace symmetri {

struct Application;
using Action = std::variant<std::function<void()>, Application>;
using TransitionActionMap = std::unordered_map<std::string, Action>;

struct Application {
 private:
  std::function<void(const std::string &t)> p;
  std::function<std::vector<Event>()> run;

 public:
  Application(const std::set<std::string> &path_to_petri,
              const TransitionActionMap &store,
              const std::string &case_id = "NOCASE", bool use_webserver = true);
  std::vector<Event> operator()() const;

  template <typename T>
  inline std::function<void(T)> push(const std::string &transition) const {
    return [transition, this](T) { p(transition); };
  }
};

}  // namespace symmetri
