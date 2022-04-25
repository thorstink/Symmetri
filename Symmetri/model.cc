#include "model.h"

namespace symmetri {

std::tuple<Model &, Transitions> run_all(Model &model) {
  model.timestamp = clock_t::now();
  Transitions T;

  for (const auto &[T_i, mut] : model.net) {
    const auto &pre = mut.first;
    if (!pre.empty() &&
        std::all_of(std::begin(pre), std::end(pre), [&](const auto &m_p) {
          return model.M[m_p] >= pre.count(m_p);
        })) {
      for (auto &m_p : pre) {
        model.M[m_p] -= 1;
      }
      T.push_back(T_i);
      model.pending_transitions.insert(T_i);
    }
  }
  return {model, T};
}

}  // namespace symmetri
