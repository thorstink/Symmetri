#pragma once
#include <set>
#include <tuple>

#include "Symmetri/types.h"
#include "json.hpp"

std::tuple<symmetri::TransitionMutation, symmetri::TransitionMutation,
           symmetri::Marking, symmetri::ArcList, symmetri::Conversions,
           symmetri::Conversions>
constructTransitionMutationMatrices(const std::set<std::string> &files);
