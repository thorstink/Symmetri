#pragma once
#include <map>
#include <set>
#include <string>

namespace symmetri {
using Place = std::string;
using Transition = std::string;

using StateNet =
    std::map<Transition, std::pair<std::multiset<Place>, std::multiset<Place>>>;
using NetMarking = std::map<Place, uint16_t>;

}  // namespace symmetri
