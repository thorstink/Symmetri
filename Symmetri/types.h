#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace symmetri {
using ArcList = std::vector<std::tuple<bool, std::string, std::string>>;
using Transition = std::string;
using Transitions = std::vector<Transition>;
using StateNet =
    std::map<std::string,
             std::pair<std::set<std::string>, std::set<std::string>>>;
using NetMarking = std::map<std::string, int16_t>;

}  // namespace symmetri
