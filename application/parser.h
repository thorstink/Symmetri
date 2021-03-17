#pragma once
#include "application.hpp"
#include "includes/json.hpp"
#include <Eigen/Dense>

using json = nlohmann::json;

std::tuple<std::unordered_map<int, Eigen::VectorXi>,
           std::unordered_map<int, Eigen::VectorXi>, std::vector<int>>
constructTransitionMutationMatrices(json net);
