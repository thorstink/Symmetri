#include "symmetri/types.h"

#include <algorithm>
#include <numeric>
namespace symmetri {

template <>
bool MarkingEquality<size_t>(const std::vector<size_t>& m1,
                             const std::vector<size_t>& m2) {
  auto m1_sorted = m1;
  auto m2_sorted = m2;
  std::sort(m1_sorted.begin(), m1_sorted.end());
  std::sort(m2_sorted.begin(), m2_sorted.end());
  return m1_sorted == m2_sorted;
}
template <>
bool MarkingReached<size_t>(const std::vector<size_t>& marking,
                            const std::vector<size_t>& final_marking) {
  if (final_marking.empty()) {
    return false;
  }
  auto unique = final_marking;
  std::sort(unique.begin(), unique.end());
  auto last = std::unique(unique.begin(), unique.end());
  unique.erase(last, unique.end());

  return std::all_of(std::begin(unique), std::end(unique), [&](const auto& p) {
    return std::count(marking.begin(), marking.end(), p) ==
           std::count(final_marking.begin(), final_marking.end(), p);
  });
}

template <>
bool MarkingEquality<std::string>(const std::vector<std::string>& m1,
                                  const std::vector<std::string>& m2) {
  auto m1_sorted = m1;
  auto m2_sorted = m2;
  std::sort(m1_sorted.begin(), m1_sorted.end());
  std::sort(m2_sorted.begin(), m2_sorted.end());
  return m1_sorted == m2_sorted;
}
template <>
bool MarkingReached<std::string>(
    const std::vector<std::string>& marking,
    const std::vector<std::string>& final_marking) {
  if (final_marking.empty()) {
    return false;
  }
  auto unique = final_marking;
  std::sort(unique.begin(), unique.end());
  auto last = std::unique(unique.begin(), unique.end());
  unique.erase(last, unique.end());

  return std::all_of(std::begin(unique), std::end(unique), [&](const auto& p) {
    return std::count(marking.begin(), marking.end(), p) ==
           std::count(final_marking.begin(), final_marking.end(), p);
  });
}

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
        constexpr auto success = "o";
        constexpr auto fail = "x";
        return n.state == State::Completed ? acc + n.transition + success
                                           : acc + fail;
      }));
}

std::string printState(symmetri::State s) noexcept {
  std::string ret;
  switch (s) {
    case State::Scheduled:
      ret = "Scheduled";
      break;
    case State::Started:
      ret = "Started";
      break;
    case State::Completed:
      ret = "Completed";
      break;
    case State::Deadlock:
      ret = "Deadlock";
      break;
    case State::UserExit:
      ret = "UserExit";
      break;
    case State::Paused:
      ret = "Paused";
      break;
    case State::Error:
      ret = "Error";
      break;
  }

  return ret;
}

}  // namespace symmetri
