#include "model.h"

namespace symmetri {

std::pair<Model, std::vector<Transition>> run_all(Model &&model) {
  Transitions T;
  for (const auto &[T_i, mut] : model.data->net) {
    const auto &pre = mut.first;
    if (!pre.empty() &&
        std::all_of(std::begin(pre), std::end(pre), [&](const auto &m_p) {
          return model.data->M[m_p] >= pre.count(m_p);
        })) {
      for (auto &m_p : pre) {
        model.data->M[m_p] -= 1;
      }
      T.push_back(T_i);
      model.data->pending_transitions.insert(T_i);
    }
  }
  return {model, T};
}

}  // namespace symmetri
