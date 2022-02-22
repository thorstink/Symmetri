#pragma once

#include <functional>
#include <set>
#include <string>
#include <unordered_map>

namespace symmetri {

using TransitionActionMap =
    std::unordered_map<std::string, std::function<void()>>;

struct Application {
 private:
  std::function<void(const std::string &t)> p;
  std::function<void()> run;

 public:
  Application(const std::set<std::string> &path_to_petri,
              const TransitionActionMap &store,
              const std::string &case_id = "NOCASE", bool use_webserver = true);
  void operator()() const;

  template <typename T>
  inline std::function<void(T)> push(const std::string &transition) const {
    return [transition, this](T) { p(transition); };
  }
};

}  // namespace symmetri
