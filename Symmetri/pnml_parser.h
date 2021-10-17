#pragma once
#include "nlohmann/json.hpp"
#include "Symmetri/types.h"
#include <tuple>

std::tuple<symmetri::TransitionMutation, symmetri::TransitionMutation, symmetri::Marking,
           nlohmann::json, std::map<uint8_t, std::string>>
constructTransitionMutationMatrices(std::string file);

nlohmann::json webMarking(const symmetri::Marking& M, const std::map<uint8_t, std::string>& index_marking_map);