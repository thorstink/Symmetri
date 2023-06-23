#pragma once
#include <set>
#include <tuple>

#include "symmetri/types.h"
namespace symmetri {

/**
 * @brief Given a set of unique paths to grml-files, it parses and merges them,
 * and returns _one_ Net, initial marking and table containing transitions with
 * their respective priority if it is not 1. If a place's initial marking is
 * defined in multiple nets, the initial marking in the last processed net is
 * used. Note that this is kind of random because a set orders the files.
 *
 * @param grml-files
 * @return std::tuple<symmetri::Net, symmetri::Marking,symmetri::PriorityTable>
 */
std::tuple<symmetri::Net, symmetri::Marking, symmetri::PriorityTable> readGrml(
    const std::set<std::string> &files);

/**
 * @brief Given a set of unique paths to pnml-files, it parses and merges them,
 * and returns _one_ Net and initial marking. If the net has priorities, these
 * will not be included in the net. If a place's initial marking is defined in
 * multiple nets, the initial marking in the last processed net is used. Note
 * that this is kind of random because a set orders the files.
 *
 * @param files
 * @return std::tuple<symmetri::Net, symmetri::Marking>
 */
std::tuple<symmetri::Net, symmetri::Marking> readPnml(
    const std::set<std::string> &files);
}  // namespace symmetri
