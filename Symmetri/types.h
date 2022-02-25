#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

namespace symmetri {
using Transition = std::string;
using Transitions = std::vector<Transition>;
using StateNet =
    std::map<std::string,
             std::pair<std::multiset<std::string>, std::multiset<std::string>>>;
using NetMarking = std::map<std::string, uint16_t>;

}  // namespace symmetri
