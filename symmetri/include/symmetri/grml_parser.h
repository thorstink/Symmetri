#pragma once
#include <set>
#include <tuple>

#include "symmetri/types.h"

/**
 * @brief Given a set of unique paths to grml-files, it parses and merges them,
 * and returns _one_ Net and initial marking. If a place's initial marking is
 * defined in multiple nets, the initial marking in the last processed net is
 * used. Note that this is kind of random because a set orders the files.
 *
 * @param files
 * @return std::tuple<symmetri::Net, symmetri::Marking>
 */
std::tuple<symmetri::Net, symmetri::Marking, symmetri::PriorityTable> readGrml(
    const std::set<std::string> &files);
