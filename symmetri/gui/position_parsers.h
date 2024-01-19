#pragma once

/** @file parsers.h */

#include <map>
#include <set>
#include <string>

namespace farbart {

// std::map<std::string, std::pair<float,float>> readGrmlPositions(
//     const std::set<std::string> &files);
std::map<std::string, std::pair<float, float>> readPnmlPositions(
    const std::set<std::string> &files);

}  // namespace farbart
