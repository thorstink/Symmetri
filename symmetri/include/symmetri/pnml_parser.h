#pragma once
#include <set>
#include <tuple>

#include "symmetri/types.h"

/**
 * @brief Given a set of unique paths to pnml-files, it parses and merges them,
 * and returns _one_ Net and initial marking.
 *
 * @param files
 * @return std::tuple<symmetri::Net, symmetri::Marking>
 */
std::tuple<symmetri::Net, symmetri::Marking> readPetriNets(
    const std::set<std::string> &files);
