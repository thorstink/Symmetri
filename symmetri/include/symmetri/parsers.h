#pragma once

/** @file parsers.h */

#include <set>
#include <tuple>

#include "symmetri/types.h"
namespace symmetri {

/**
 * @brief Given a set of unique paths to grml-files, it parses and merges them,
 * and returns _one_ Net, initial marking and table containing transitions with
 * their respective priority if it is not 1. If a place's initial marking is
 * defined in multiple nets, the initial marking in the last processed net is
 * used. Note that this is kind of random because a set orders the files. It
 * will also register tokens for the color attributes that are in the arcs that
 * go from a place to a transition.
 *
 * @param grml-files
 * @return std::tuple<Net, Marking,PriorityTable>
 */
std::tuple<Net, Marking, PriorityTable> readGrml(
    const std::set<std::string> &files);

/**
 * @brief Given a set of unique paths to pnml-files, it parses and merges them,
 * and returns _one_ Net and initial marking. If the net has priorities, these
 * will not be included in the net. If a place's initial marking is defined in
 * multiple nets, the initial marking in the last processed net is used. Note
 * that this is kind of random because a set orders the files. It will also
 * register tokens for the color attributes that are in the arcs that go from a
 * place to a transition.
 *
 * @param pnml-files
 * @return std::tuple<Net, Marking>
 */
std::tuple<Net, Marking> readPnml(const std::set<std::string> &files);

}  // namespace symmetri
