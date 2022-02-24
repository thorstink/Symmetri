#include "model.h"

#include <optional>
#include <unordered_map>

namespace symmetri {

Model run_all(Model model) {
  Transitions T;

  for (const auto &[T_i, mut] : model.data->net) {
    const auto &pre = mut.first;
    if (!pre.empty() &&
        std::all_of(std::begin(pre), std::end(pre),
                    [&](const auto &m_p) { return model.data->M[m_p] > 0; })) {
      for (auto &m_p : pre) {
        model.data->M[m_p] -= 1;
      }
      T.push_back(T_i);
      model.data->active_transitions.insert(T_i);
    }
  }

  model.transitions_->enqueue_bulk(T.begin(), T.size());

  return model;
}

}  // namespace symmetri
