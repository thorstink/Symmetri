#include "model.hpp"
#include <iostream>
#include <mpark/patterns.hpp>

namespace model {

using namespace mpark::patterns;

std::pair<Model, Transitions> run(Model model) {
  auto &M = model.data->M;
  const auto &Dm = model.data->Dm;
  Transitions T;

  // for_let(pattern(ds(arg, arg)) = Dm) = [&M, &T](const auto &Tx,
  //                                                const auto &dM) {
  //   WHEN(((M - dM).array() >= 0).all()) {
  //     M -= dM;
  //     T.push_back(Tx);
  //   };
  // };

  for (const auto &[T_i, dM_i] : Dm) {
    if (((M - dM_i).array() >= 0).all()) {
      M -= dM_i;
      T.push_back(T_i);
    }
  }

  return {model, T};
}

} // namespace model