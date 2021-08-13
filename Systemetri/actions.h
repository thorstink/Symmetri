#pragma once
#include "model.h"
#include <rxcpp/rx.hpp>

namespace actions {
using namespace rxcpp;

using r_type = std::function<rxcpp::observable<model::Reducer>(types::Transition)>;

r_type executeTransition(const observe_on_one_worker &scheduler,
                       const model::TransitionActionMap &local_store);

} // namespace actions
