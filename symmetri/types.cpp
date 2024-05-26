#include "symmetri/types.h"

#include <algorithm>
#include <numeric>

#include "symmetri/colors.hpp"
namespace symmetri {

bool isSynchronous(const DirectMutation&) { return true; }
Token fire(const DirectMutation&) { return Color::Success; }

std::string Color::toString(Token r) { return Color::map[r]; }

Token Color::registerToken(const std::string& s) {
  return Color::map.insert({impl::HashColor(s.c_str()), s}).first->first;
}

std::unordered_map<Token, std::string> Color::getColors() { return Color::map; }

bool stateNetEquality(const Net& net1, const Net& net2) {
  if (net1.size() != net2.size()) {
    return false;
  }
  for (const auto& [t1, mut1] : net1) {
    if (net2.find(t1) != net2.end()) {
      for (const auto& pre : mut1.first) {
        if (mut1.first.size() != net2.at(t1).first.size()) {
          return false;
        }
        if (std::count(std::begin(mut1.first), std::end(mut1.first), pre) !=
            std::count(std::begin(net2.at(t1).first),
                       std::end(net2.at(t1).first), pre)) {
          return false;
        }
      }
      for (const auto& post : mut1.second) {
        if (mut1.second.size() != net2.at(t1).second.size()) {
          return false;
        }
        if (std::count(std::begin(mut1.second), std::end(mut1.second), post) !=
            std::count(std::begin(net2.at(t1).second),
                       std::end(net2.at(t1).second), post)) {
          return false;
        }
      }
    } else {
      return false;
    }
  }

  return true;
}

size_t calculateTrace(const Eventlog& event_log) noexcept {
  // calculate a trace-id, in a simple way.
  return std::hash<std::string>{}(std::accumulate(
      event_log.begin(), event_log.end(), std::string(""),
      [](const auto& acc, const Event& n) {
        return acc + n.case_id + n.transition + Color::toString(n.state);
      }));
}

}  // namespace symmetri
