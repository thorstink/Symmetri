
#include "petri.h"
namespace symmetri {

size_t toIndex(const std::vector<std::string> &m, const std::string &s) {
  auto ptr = std::find(m.begin(), m.end(), s);
  return std::distance(m.begin(), ptr);
}

bool canFire(const SmallVectorInput &pre, std::vector<AugmentedToken> &tokens) {
  for (const auto &m_p : pre) {
    const auto required = std::count(pre.begin(), pre.end(), m_p);
    int actual = 0;
    for (auto it = std::find(tokens.begin(), tokens.end(), m_p);
         it != tokens.end(); actual++) {
      if (actual == required) {
        break;  // required tokens available!
      } else {
        it = std::find(++it, tokens.end(), m_p);
      }
    }
    // if for any of the inputs there are not sufficient tokens, the transition
    // can not fire.
    if (actual < required) {
      return false;
    }
  }

  // if we did not return early and pre is not empty, it the transition is
  // fireable.
  return not pre.empty();
}

gch::small_vector<size_t, 32> possibleTransitions(
    const std::vector<AugmentedToken> &tokens,
    const std::vector<SmallVectorInput> &input_n,
    const std::vector<SmallVector> &p_to_ts_n) {
  gch::small_vector<size_t, 32> possible_transition_list_n;
  for (const AugmentedToken &place : tokens) {
    // transition index
    for (const size_t &t : p_to_ts_n[std::get<size_t>(place)]) {
      // transition is not already considered:
      const bool is_unique = std::find(possible_transition_list_n.cbegin(),
                                       possible_transition_list_n.cend(),
                                       t) == possible_transition_list_n.cend();
      if (not is_unique) {
        continue;  // already in the list, so skip
      }
      const auto &inputs = input_n[t];
      const bool is_input =
          std::find(inputs.cbegin(), inputs.cend(), place) != inputs.cend();
      if (not is_input) {
        continue;  // place is not an input of transition t, so skip
      }

      // current colored place is an input of t
      possible_transition_list_n.push_back(t);
    }
  }

  return possible_transition_list_n;
}

}  // namespace symmetri
