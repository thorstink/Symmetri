#pragma once
#include "nlohmann/json.hpp"
#include "types.h"
#include <tuple>

std::tuple<types::TransitionMutation, types::TransitionMutation, types::Marking,
           nlohmann::json, std::map<uint8_t, std::string>>
constructTransitionMutationMatrices(std::string file);

nlohmann::json webMarking(const types::Marking& M, const std::map<uint8_t, std::string>& index_marking_map);