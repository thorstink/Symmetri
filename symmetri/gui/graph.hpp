#pragma once

#include <math.h>  // fmodf
#include <ogdf/basic/GraphAttributes.h>
#include <ogdf/energybased/FMMMLayout.h>
#include <ogdf/fileformats/GraphIO.h>
#include <ogdf/layered/MedianHeuristic.h>
#include <ogdf/layered/OptimalHierarchyLayout.h>
#include <ogdf/layered/OptimalRanking.h>
#include <ogdf/layered/SugiyamaLayout.h>
#include <ogdf/orthogonal/OrthoLayout.h>
#include <ogdf/planarity/EmbedderMinDepthMaxFaceLayers.h>
#include <ogdf/planarity/PlanarSubgraphFast.h>
#include <ogdf/planarity/PlanarizationLayout.h>
#include <ogdf/planarity/SubgraphPlanarizer.h>
#include <ogdf/planarity/VariableEmbeddingInserter.h>
#include <symmetri/types.h>

#include <algorithm>
#include <iostream>

#include "imgui.h"
#include "symbol.h"

template <typename T>
size_t toIndex(const std::vector<T>& m,
               const std::function<bool(const T&)>& s) {
  auto ptr = std::find_if(m.begin(), m.end(), s);
  return ptr->id.index();
}

struct Node {
  std::string name;
  Symbol id;
  ImVec2 Pos;
  static ImVec2 GetCenterPos(const ImVec2& pos, const ImVec2& size) {
    return ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
  }
};

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

inline std::shared_ptr<Graph> createGraph(const symmetri::Net net) {
  std::vector<Node> nodes;
  std::vector<Arc> arcs;
  ogdf::Graph G;
  ogdf::GraphAttributes GA(G, ogdf::GraphAttributes::nodeGraphics |
                                  ogdf::GraphAttributes::nodeLabel |
                                  ogdf::GraphAttributes::edgeGraphics);
  std::vector<ogdf::node> ogdf_places, ogdf_transitions, ogdf_nodes;

  for (const auto& [t, io] : net) {
    for (const auto& s : io.first) {
      if (std::find_if(nodes.begin(), nodes.end(), [&](const auto& n) {
            return s.first == n.name;
          }) == std::end(nodes)) {
        const auto place_key = Symbol('P', nodes.size());
        ogdf_nodes.push_back(G.newNode());
        auto current_node = ogdf_nodes.back();
        GA.label(current_node) = s.first;
        GA.shape(current_node) = ogdf::Shape::Ellipse;
        nodes.push_back({s.first, place_key});
      }
    }

    for (const auto& s : io.second) {
      if (std::find_if(nodes.begin(), nodes.end(), [&](const auto& n) {
            return s.first == n.name;
          }) == std::end(nodes)) {
        const auto place_key = Symbol('P', nodes.size());
        ogdf_nodes.push_back(G.newNode());
        auto current_node = ogdf_nodes.back();
        GA.label(current_node) = s.first;
        GA.shape(current_node) = ogdf::Shape::Ellipse;
        nodes.push_back({s.first, place_key});
      }
    }

    const auto transition_key = Symbol('T', nodes.size());
    ogdf_nodes.push_back(G.newNode());
    auto current_node = ogdf_nodes.back();
    GA.label(current_node) = t;
    nodes.push_back({t, transition_key});
  }

  // make the graph
  for (const auto& x : net) {
    const auto [t, io] = x;
    const auto transition_idx =
        toIndex<Node>(nodes, [=](const Node& n) { return x.first == n.name; });
    for (const auto& s : io.first) {
      const auto place_idx = toIndex<Node>(
          nodes, [=](const Node& n) { return s.first == n.name; });
      G.newEdge(ogdf_nodes[place_idx], ogdf_nodes[transition_idx]);
      arcs.push_back({s.second, {place_idx, transition_idx}});
    }

    for (const auto& s : io.second) {
      const auto place_idx = toIndex<Node>(
          nodes, [=](const Node& n) { return s.first == n.name; });
      G.newEdge(ogdf_nodes[transition_idx], ogdf_nodes[place_idx]);
      arcs.push_back({s.second, {transition_idx, place_idx}});
    }
  }

  {
    using namespace ogdf;
    SugiyamaLayout SL;
    SL.setRanking(new OptimalRanking);
    SL.setCrossMin(new MedianHeuristic);

    OptimalHierarchyLayout* ohl = new OptimalHierarchyLayout;
    SL.setLayout(ohl);

    SL.call(GA);
    GA.rotateLeft90();
    GA.translateToNonNeg();
  }

  for (size_t i = 0; i < ogdf_nodes.size(); i++) {
    nodes[i].Pos = ImVec2(GA.x(ogdf_nodes[i]), 2 * GA.y(ogdf_nodes[i]));
  }

  std::vector<size_t> v(arcs.size()), w(nodes.size());
  std::iota(v.begin(), v.end(), 0);
  std::iota(w.begin(), w.end(), 0);
  return std::make_shared<Graph>(
      Graph{std::move(arcs), std::move(nodes), std::move(v), std::move(w)});
}
