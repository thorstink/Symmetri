#pragma once
#include <set>
#include <tuple>

#include "types.h"

std::tuple<symmetri::StateNet, symmetri::NetMarking>
constructTransitionMutationMatrices(const std::set<std::string> &files);
