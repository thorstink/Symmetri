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
struct Graph {
  struct Node {
    std::string name;
    ogdf::node node;
    Symbol id;
    ImVec2 Pos, Size;

    ImVec2 GetCenterPos() const {
      return ImVec2(Pos.x + Size.x * 0.5f, Pos.y + Size.y * 0.5f);
    }
  };

  struct Arc {
    ogdf::edge edge;
    symmetri::Token color;
    std::array<Node*, 2> from_to;
  };

  ogdf::Graph G;

  explicit Graph(const symmetri::Net net, const symmetri::Marking&) {
    ogdf::GraphAttributes GA(G, ogdf::GraphAttributes::nodeGraphics |
                                    ogdf::GraphAttributes::nodeLabel |
                                    ogdf::GraphAttributes::edgeGraphics);
    for (const auto& [t, io] : net) {
      for (const auto& s : io.first) {
        const auto place_key = Symbol('P', nodes.size() + 1);

        if (std::find_if(nodes.begin(), nodes.end(), [&](const auto& n) {
              return s.first == n.name;
            }) == std::end(nodes)) {
          nodes.push_back({s.first, G.newNode(), place_key});
          auto& current_node = nodes.back();
          GA.label(current_node.node) = current_node.name;
          GA.shape(current_node.node) = ogdf::Shape::Ellipse;
        }
      }

      for (const auto& s : io.second) {
        const auto place_key = Symbol('P', nodes.size() + 1);
        if (std::find_if(nodes.begin(), nodes.end(), [&](const auto& n) {
              return s.first == n.name;
            }) == std::end(nodes)) {
          nodes.push_back({s.first, G.newNode(), place_key});
          auto& current_node = nodes.back();
          GA.label(current_node.node) = current_node.name;
          GA.shape(current_node.node) = ogdf::Shape::Ellipse;
        }
      }

      const auto transition_key = Symbol('T', nodes.size() + 1);
      nodes.push_back(Node{t, G.newNode(), transition_key});
      GA.label(nodes.back().node) = nodes.back().name;
    }

    // make the graph
    for (const auto& x : net) {
      const auto [t, io] = x;
      const auto transition =
          std::find_if(nodes.begin(), nodes.end(),
                       [&](const auto& n) { return x.first == n.name; });

      for (const auto& s : io.first) {
        const auto place =
            std::find_if(nodes.begin(), nodes.end(),
                         [&](const auto& n) { return s.first == n.name; });
        arcs.push_back({G.newEdge(place->node, transition->node),
                        s.second,
                        {&(*place), &(*transition)}});
        // GA.bends(arcs.back().edge);
      }

      for (const auto& s : io.second) {
        const auto place =
            std::find_if(nodes.begin(), nodes.end(),
                         [&](const auto& n) { return s.first == n.name; });
        arcs.push_back({G.newEdge(transition->node, place->node),
                        s.second,
                        {&(*transition), &(*place)}});
        // GA.bends(arcs.back().edge);
      }
    }

    {
      using namespace ogdf;
      for (node v : G.nodes) {
        GA.width(v) = 150.0;
        GA.height(v) = 25.0;
      }

      // PlanarizationLayout pl;

      // SubgraphPlanarizer* crossMin = new SubgraphPlanarizer;
      // PlanarSubgraphFast<int>* ps = new PlanarSubgraphFast<int>;
      // ps->runs(100);
      // VariableEmbeddingInserter* ves = new VariableEmbeddingInserter;
      // ves->removeReinsert(RemoveReinsertType::All);

      // crossMin->setSubgraph(ps);
      // crossMin->setInserter(ves);
      // pl.setCrossMin(crossMin);

      // EmbedderMinDepthMaxFaceLayers* emb = new EmbedderMinDepthMaxFaceLayers;
      // pl.setEmbedder(emb);

      // OrthoLayout* ol = new OrthoLayout;
      // ol->separation(40.0);
      // ol->cOverhang(0.4);
      // pl.setPlanarLayouter(ol);

      // pl.call(GA);
      SugiyamaLayout SL;
      SL.setRanking(new OptimalRanking);
      SL.setCrossMin(new MedianHeuristic);

      OptimalHierarchyLayout* ohl = new OptimalHierarchyLayout;
      // ohl->layerDistance(30.0);
      // ohl->nodeDistance(25.0);
      // ohl->weightBalancing(0.8);
      SL.setLayout(ohl);

      SL.call(GA);
      GA.rotateLeft90();
      GA.translateToNonNeg();
      GraphIO::write(GA, "output-unix-history-hierarchical.svg",
                     GraphIO::drawSVG);
    }

    for (auto& n : nodes) {
      n.Pos = ImVec2(GA.x(n.node), GA.y(n.node));
    }
  }

  std::vector<Node> nodes;
  std::vector<Arc> arcs;
};
