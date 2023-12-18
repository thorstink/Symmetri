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
  return std::distance(m.begin(), ptr);
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
  std::array<ImVec2*, 2> from_to_pos;
};

struct Graph {
  explicit Graph(const symmetri::Net net, const symmetri::Marking&) {
    ogdf::Graph G;
    ogdf::GraphAttributes GA(G, ogdf::GraphAttributes::nodeGraphics |
                                    ogdf::GraphAttributes::nodeLabel |
                                    ogdf::GraphAttributes::edgeGraphics);
    std::vector<ogdf::node> ogdf_places, ogdf_transitions;

    for (const auto& [t, io] : net) {
      for (const auto& s : io.first) {
        if (std::find_if(places.begin(), places.end(), [&](const auto& n) {
              return s.first == n.name;
            }) == std::end(places)) {
          const auto place_key = Symbol('P', places.size() + 1);
          ogdf_places.push_back(G.newNode());
          auto current_node = ogdf_places.back();
          GA.label(current_node) = s.first;
          GA.shape(current_node) = ogdf::Shape::Ellipse;
          places.push_back({s.first, place_key});
        }
      }

      for (const auto& s : io.second) {
        if (std::find_if(places.begin(), places.end(), [&](const auto& n) {
              return s.first == n.name;
            }) == std::end(places)) {
          const auto place_key = Symbol('P', places.size() + 1);
          ogdf_places.push_back(G.newNode());
          auto current_node = ogdf_places.back();
          GA.label(current_node) = s.first;
          GA.shape(current_node) = ogdf::Shape::Ellipse;
          places.push_back({s.first, place_key});
        }
      }

      const auto transition_key = Symbol('T', transitions.size() + 1);
      ogdf_transitions.push_back(G.newNode());
      auto current_node = ogdf_transitions.back();
      GA.label(current_node) = t;
      transitions.push_back({t, transition_key});
    }

    // make the graph
    for (const auto& x : net) {
      const auto [t, io] = x;
      const auto transition_idx = toIndex<Node>(
          transitions, [=](const Node& n) { return x.first == n.name; });
      for (const auto& s : io.first) {
        const auto place_idx = toIndex<Node>(
            places, [=](const Node& n) { return s.first == n.name; });
        // ogdf_arcs.push_back();
        G.newEdge(ogdf_places[place_idx], ogdf_transitions[transition_idx]);
        arcs.push_back(
            {s.second,
             {&(places[place_idx].Pos), &(transitions[transition_idx].Pos)}});
      }

      for (const auto& s : io.second) {
        const auto place_idx = toIndex<Node>(
            places, [=](const Node& n) { return s.first == n.name; });
        G.newEdge(ogdf_transitions[transition_idx], ogdf_places[place_idx]);
        arcs.push_back(
            {s.second,
             {&(transitions[transition_idx].Pos), &(places[place_idx].Pos)}});
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
      GraphIO::write(GA, "output-unix-history-hierarchical.svg",
                     GraphIO::drawSVG);
    }

    for (size_t i = 0; i < ogdf_places.size(); i++) {
      places[i].Pos = ImVec2(GA.x(ogdf_places[i]), 2 * GA.y(ogdf_places[i]));
    }
    for (size_t i = 0; i < ogdf_transitions.size(); i++) {
      transitions[i].Pos =
          ImVec2(GA.x(ogdf_transitions[i]), 2 * GA.y(ogdf_transitions[i]));
    }
  }

  std::vector<Node> places;
  std::vector<Node> transitions;
  std::vector<Arc> arcs;
};
