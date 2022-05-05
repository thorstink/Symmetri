#pragma once
#include <set>
#include <tuple>

#include "types.h"

std::tuple<symmetri::StateNet, symmetri::NetMarking>
readPetriNets(const std::set<std::string> &files);
