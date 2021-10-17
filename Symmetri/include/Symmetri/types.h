#pragma once

#include "Symmetri/expected.hpp"
#include <Eigen/SparseCore>
#include <optional>
#include <string>
#include <unordered_map>

namespace symmetri {
using Transition = std::string;
using Marking = Eigen::SparseVector<int16_t>;
using MarkingMutation = Eigen::SparseVector<int16_t>;
using TransitionMutation = std::unordered_map<Transition, MarkingMutation>;
using OptionalError = std::optional<MarkingMutation>;
using Transitions = std::vector<Transition>;
using TransitionActionMap =
    std::unordered_map<Transition, std::function<OptionalError()>>;

} // namespace symmetri
