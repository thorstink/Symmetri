#pragma once

#include <math.h>  // fmodf
#include <symmetri/types.h>

#include <algorithm>
#include <array>
#include <map>
#include <numeric>

#include "imgui.h"

template <typename T>
size_t toIndex(const std::vector<T>& m,
               const std::function<bool(const T&)>& s) {
  return std::distance(m.begin(), std::find_if(m.begin(), m.end(), s));
}

struct Node {
  std::string name;
  enum Type { Place, Transition };
  Type type = Place;
  ImVec2 Pos;
  static ImVec2 GetCenterPos(const ImVec2& pos, const ImVec2& size) {
    return ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
  }
};

inline ImVec2 toImVec2(const std::pair<float, float>& p) {
  return ImVec2(p.first, p.second);
}

struct Arc {
  symmetri::Token color;
  std::array<size_t, 2> from_to_pos_idx;
};

struct Graph {
  std::vector<Arc> arcs = {};
  std::vector<Node> nodes = {};
  std::vector<size_t> a_idx = {};
  std::vector<size_t> n_idx = {};

  void reset(const Graph& g) {
    const auto offset_arcs = arcs.size();
    const auto offset_nodes = nodes.size();
    nodes.insert(nodes.end(), g.nodes.begin(), g.nodes.end());
    arcs.insert(arcs.end(), g.arcs.begin(), g.arcs.end());
    a_idx = g.a_idx;
    n_idx = g.n_idx;
    for (auto& idx : a_idx) {
      idx += offset_arcs;
    }
    for (auto& idx : n_idx) {
      idx += offset_nodes;
    }
    for (auto& arc : arcs) {
      arc.from_to_pos_idx[0] += offset_nodes;
      arc.from_to_pos_idx[1] += offset_nodes;
    }
  }
};

inline std::shared_ptr<Graph> createGraph(
    const symmetri::Net net, const std::map<std::string, ImVec2>& positions) {
  std::vector<Node> nodes;
  std::vector<Arc> arcs;

  for (const auto& [t, io] : net) {
    for (const auto& s : io.first) {
      if (std::find_if(nodes.begin(), nodes.end(), [&](const auto& n) {
            return s.first == n.name;
          }) == std::end(nodes)) {
        nodes.push_back({s.first, Node::Type::Place, positions.at(s.first)});
      }
    }

    for (const auto& s : io.second) {
      if (std::find_if(nodes.begin(), nodes.end(), [&](const auto& n) {
            return s.first == n.name;
          }) == std::end(nodes)) {
        nodes.push_back({s.first, Node::Type::Place, positions.at(s.first)});
      }
    }

    nodes.push_back({t, Node::Type::Transition, positions.at(t)});
  }

  // make the graph
  for (const auto& x : net) {
    const auto [t, io] = x;
    const auto transition_idx =
        toIndex<Node>(nodes, [=](const Node& n) { return x.first == n.name; });
    for (const auto& s : io.first) {
      const auto place_idx = toIndex<Node>(
          nodes, [=](const Node& n) { return s.first == n.name; });
      arcs.push_back({s.second, {place_idx, transition_idx}});
    }

    for (const auto& s : io.second) {
      const auto place_idx = toIndex<Node>(
          nodes, [=](const Node& n) { return s.first == n.name; });
      arcs.push_back({s.second, {transition_idx, place_idx}});
    }
  }

  std::vector<size_t> v(arcs.size()), w(nodes.size());
  std::iota(v.begin(), v.end(), 0);
  std::iota(w.begin(), w.end(), 0);
  return std::make_shared<Graph>(
      Graph{std::move(arcs), std::move(nodes), std::move(v), std::move(w)});
}
