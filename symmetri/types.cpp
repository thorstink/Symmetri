#include "symmetri/types.h"

#include <algorithm>
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

bool StateNetEquality(const StateNet& net1, const StateNet& net2) {
  if (net1.size() != net2.size()) {
    return false;
  }
  for (const auto& [t1, mut1] : net1) {
    if (net2.contains(t1)) {
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
}  // namespace symmetri
