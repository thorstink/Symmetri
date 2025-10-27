#pragma once

/** @file parsers.h */

#include <map>
#include <set>
#include <string>

#include "model.h"

namespace farbart {

std::map<std::string, model::Coordinate> readPnmlPositions(
    const std::set<std::string>& files);

std::map<std::string, model::Coordinate> readGrmlPositions(
    const std::set<std::string>& files);
}  // namespace farbart
