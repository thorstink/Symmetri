#include "model.h"
#include <optional>

namespace symmetri {

bool transition_enabled(Marking M, Marking dM) {
  bool enabled = true;
  for (Eigen::SparseVector<int16_t>::InnerIterator it(dM); it; ++it) {
    if (M.coeffRef(it.index()) < it.value()) {
      enabled = false;
      break;
    }
  }
  return enabled;
}

Model run_all(Model model) {
  auto &M = model.data->M;
  const auto &Dm = model.data->Dm;
  Transitions T;

  for (const auto &[T_i, dM_i] : Dm) {
    if (transition_enabled(M, dM_i)) {
      M -= dM_i;
      T.push_back(T_i);
      model.data->active_transitions.insert(T_i);
    }
  }
  model.transitions_->enqueue_bulk(T.begin(), T.size());

  return model;
}

std::optional<Model> run_one(Model model) {
  auto &M = model.data->M;
  const auto &Dm = model.data->Dm;

  auto iterator = std::find_if(Dm.begin(), Dm.end(), [&M](const auto &tup) {
    const auto &[T_i, dM_i] = tup;
    return transition_enabled(M, dM_i);
  });

  if (iterator != Dm.end()) {
    const auto &[T_i, dM_i] = *iterator;
    M -= dM_i;
    model.data->active_transitions.insert(T_i);
    model.transitions_->enqueue(T_i);
    return model;
  } else {
    return std::nullopt;
  }
}
} // namespace symmetri
