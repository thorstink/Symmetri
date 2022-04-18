#pragma once
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "Symmetri/symmetri.h"

namespace symmetri {
using Place = std::string;
using Transitions = std::vector<Transition>;

using StateNet =
    std::map<Transition, std::pair<std::multiset<Place>, std::multiset<Place>>>;
using NetMarking = std::map<Place, uint16_t>;

}  // namespace symmetri
