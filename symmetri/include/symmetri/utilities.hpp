#pragma once

/** @file utilities.hpp */

#include <algorithm>

#include "symmetri/types.h"

namespace symmetri {

/**
 * @brief Checks if the markings are exactly the same. Note that this uses a
 * different type for Marking compared to the Marking type used to
 * construct a net (an unordered map of strings). In this format the amount of
 * tokens in a particular place is represented by how often that place occurs in
 * the vector. For example: {"A","A","B"} is a marking with two tokens in place
 * "A" and one token in place "B". This format does not have the overhead of
 * mentioning all empty places.
 *
 * @tparam T
 * @param m1
 * @param m2
 * @return true
 * @return false
 */
template <typename T>
bool MarkingEquality(const std::vector<T>& m1, const std::vector<T>& m2) {
  auto m1_sorted = m1;
  auto m2_sorted = m2;
  std::sort(m1_sorted.begin(), m1_sorted.end());
  std::sort(m2_sorted.begin(), m2_sorted.end());
  return m1_sorted == m2_sorted;
}

/**
 * @brief Checks if marking is at least a subset of final_marking. Note
 * that this uses a different type for Marking compared to the Marking
 * type used to construct a net (an unordered map of strings). In this
 * format the amount of tokens in a particular place is represented by how often
 * that place occurs in the vector. For example: {"A","A","B"} is a marking with
 * two tokens in place "A" and one token in place "B". This format does not have
 * the overhead of mentioning all empty places.
 *
 * @tparam T
 * @param marking
 * @param final_marking
 * @return true
 * @return false
 */
template <typename T>
bool MarkingReached(const std::vector<T>& marking,
                    const std::vector<T>& final_marking) {
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

/**
 * @brief Checks if two petri-nets have equal amount of arcs between places
 * and transitions of the same name.
 *
 * @param net1
 * @param net2
 * @return true
 * @return false
 */
bool stateNetEquality(const Net& net1, const Net& net2);

/**
 * @brief Calculates a hash given an event log. This hash is only influenced by
 * the order of the completions of transitions, the tokens the transitions
 * return and the case_id of the net the transition is fired from.
 *
 * @param event_log An eventlog, can both be from a terminated or a still active
 * net.
 * @return size_t The hashed result.
 */
size_t calculateTrace(const Eventlog& event_log) noexcept;

}  // namespace symmetri
