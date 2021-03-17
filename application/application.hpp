#pragma once

#include "transitions.hpp"
#include <Eigen/Dense>
#include <unordered_map>
namespace application {
using namespace transitions;
using Marking = Eigen::VectorXi;
using TransitionMutation = std::unordered_map<Transition, Marking>;

} // namespace application
