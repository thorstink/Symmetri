#pragma once

/** @file parsers.h */

#include <map>
#include <set>
#include <string>
#define IMGUI_DEFINE_MATH_OPERATORS

#include "imgui.h"

namespace farbart {

// std::map<std::string, std::pair<float,float>> readGrmlPositions(
//     const std::set<std::string> &files);
std::map<std::string, ImVec2> readPnmlPositions(
    const std::set<std::string> &files);

}  // namespace farbart
