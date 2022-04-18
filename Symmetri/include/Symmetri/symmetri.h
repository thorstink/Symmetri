#pragma once

#include <signal.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <set>
#include <string>
#include <unordered_map>
#include <variant>
// using namespace std;
// Define the function to be called when ctrl-c (SIGINT) is sent to process

namespace {
static bool EXIT = false;
std::function<void(int)> shutdown_handler([](int) { EXIT = true; });
inline void signal_handler(int signal) { shutdown_handler(signal); }
}  // namespace

namespace symmetri {

struct Application;
using Transition = std::string;
using clock_t = std::chrono::system_clock;

enum class TransitionState { Started, Completed, Error };

using Event =
    std::tuple<std::string, Transition, TransitionState, clock_t::time_point>;

typedef std::vector<Event> (*loggedFunction)();
typedef void (*nonLoggedFunction)();

using Action = std::variant<loggedFunction, nonLoggedFunction, Application>;

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
