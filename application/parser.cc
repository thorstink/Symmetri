#include "parser.h"
#include <algorithm>
#include <iostream>
#include <ranges>
#include <regex>
#include <set>
#include <string_view>
#include <unordered_map>

using namespace application;

std::vector<std::string> getSplits(const std::string &v) {
  std::string s(v);
  std::regex delim(R"/(,)/");
  std::cregex_token_iterator cursor(&s[0], &s[0] + s.size(), delim, {-1, 1});
  std::cregex_token_iterator end;
  return std::vector<std::string>(cursor, end);
}

std::tuple<std::unordered_map<int, Eigen::VectorXi>,
           std::unordered_map<int, Eigen::VectorXi>, std::vector<int>>
constructTransitionMutationMatrices(json net) {
  std::sort(net["places"].begin(), net["places"].end());
  std::sort(net["trans"].begin(), net["trans"].end());

  std::vector<std::string> places, transitions;
  std::vector<int> initial_marking, places_indices, transitions_indices;

  std::set<std::pair<std::string, std::string>> arcs;
  std::unordered_map<std::string, int> place_index_map;
  std::unordered_map<std::string, int> transition_index_map;

  for (const auto &v : net["places"]) {
    auto splits = getSplits(v);
    places.push_back(splits[0]);
    try {
      initial_marking.push_back(std::stoi(splits[6]));
    } catch (const std::exception &e) {
      std::cerr << e.what() << ": " << splits[6]
                << "is not a valid value for an initial place marking" << '\n';
    }
  }

  for (const auto &v : net["trans"]) {
    auto splits = getSplits(v);
    transitions.push_back(splits[0]);
  }

  for (const auto &v : net["arcs"]) {
    auto splits = getSplits(v);
    arcs.insert({splits[0], splits[2]});
  }

  places_indices.resize(places.size());
  transitions_indices.resize(transitions.size());
  std::iota(places_indices.begin(), places_indices.end(), 0);
  std::transform(places.begin(), places.end(), places_indices.begin(),
                 std::inserter(place_index_map, place_index_map.end()),
                 [](std::string s, int i) { return std::make_pair(s, i); });
  std::iota(transitions_indices.begin(), transitions_indices.end(), 0);
  std::transform(
      transitions.begin(), transitions.end(), transitions_indices.begin(),
      std::inserter(transition_index_map, transition_index_map.end()),
      [](std::string s, int i) { return std::make_pair(s, i); });

  Eigen::Matrix<int, Eigen::Dynamic, Eigen::Dynamic> Dp(places.size(),
                                                        transitions.size()),
      Dm(places.size(), transitions.size());

  Dp.setZero();
  Dm.setZero();

  for (auto [from, to] : arcs) {
    if (place_index_map.contains(from)) {
      // (D-) matrix is m * n where m amount of transitions and n
      // is the number of places. (D-)ij = 1 if transition i has
      // _input_ from place j, 0 otherwise.
      Dm(place_index_map.at(from), transition_index_map.at(to)) += 1;
    } else {
      // (D+) matrix matrix is m * n where m amount of transitions
      // and n is the number of places. (D+)ij = 1 if transition i
      // has _output_ from place j, 0 otherwise.
      Dp(place_index_map.at(to), transition_index_map.at(from)) += 1;
    }
  }

  int transition_count = transitions.size();
  std::unordered_map<int, Eigen::VectorXi> pre_map, post_map;
  for (int i = 0; i < transition_count; ++i) {
    Eigen::VectorXi T(transition_count);
    T.setZero();
    T(i) = 1;
    pre_map.insert({i, (Dm * T).eval()});
  }

  for (int i = 0; i < transition_count; ++i) {
    Eigen::VectorXi T(transition_count);
    T.setZero();
    T(i) = 1;
    post_map.insert({i, (Dp * T).eval()});
  }

  return {pre_map, post_map, initial_marking};
}
