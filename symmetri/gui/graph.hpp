#pragma once

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

#include "imgui_node_editor.h"
#include "redux.hpp"
#include "symbol.h"

struct Graph {
  struct Node {
    std::string name;
    ogdf::node node;
    Symbol id;
    Symbol input;
    Symbol output;
    double x, y;
  };
  struct Arc {
    ogdf::edge edge;
    Symbol from, to;
  };
  ogdf::Graph G;
  ax::NodeEditor::Config config;
  ax::NodeEditor::EditorContext* m_Context;

  explicit Graph(const symmetri::Net net, const symmetri::Marking&) {
    config.SettingsFile = "Simple.json";
    m_Context = ax::NodeEditor::CreateEditor(&config);
    ogdf::GraphAttributes GA(G, ogdf::GraphAttributes::nodeGraphics |
                                    ogdf::GraphAttributes::nodeLabel |
                                    ogdf::GraphAttributes::edgeGraphics);
    for (const auto& [t, io] : net) {
      const auto transition_key = Symbol('t', transitions.size() + 1);
      const auto transition_input_key = Symbol('a', transitions.size() + 1);
      const auto transition_output_key = Symbol('b', transitions.size() + 1);

      for (const auto& s : io.first) {
        const auto place_key = Symbol('p', places.size() + 1);
        const auto place_input_key = Symbol('c', places.size() + 1);
        const auto place_output_key = Symbol('d', places.size() + 1);

        if (std::find_if(places.begin(), places.end(), [&](const auto& n) {
              return s.first == n.name;
            }) == std::end(places)) {
          places.push_back({s.first, G.newNode(), place_key, place_input_key,
                            place_output_key});
          auto& current_node = places.back();
          GA.label(current_node.node) = current_node.name;
          GA.shape(current_node.node) = ogdf::Shape::Ellipse;
        }
      }

      for (const auto& s : io.second) {
        const auto place_key = Symbol('p', places.size() + 1);
        const auto place_input_key = Symbol('c', places.size() + 1);
        const auto place_output_key = Symbol('d', places.size() + 1);

        if (std::find_if(places.begin(), places.end(), [&](const auto& n) {
              return s.first == n.name;
            }) == std::end(places)) {
          places.push_back({s.first, G.newNode(), place_key, place_input_key,
                            place_output_key});
          auto& current_node = places.back();
          GA.label(current_node.node) = current_node.name;
          GA.shape(current_node.node) = ogdf::Shape::Ellipse;
        }
      }
      transitions.push_back(Node{t, G.newNode(), transition_key,
                                 transition_input_key, transition_output_key});
      GA.label(transitions.back().node) = transitions.back().name;
    }

    // make the graph
    for (const auto& x : net) {
      const auto [t, io] = x;
      const auto transition =
          std::find_if(transitions.begin(), transitions.end(),
                       [&](const auto& n) { return x.first == n.name; });

      for (const auto& s : io.first) {
        const auto place =
            std::find_if(places.begin(), places.end(),
                         [&](const auto& n) { return s.first == n.name; });
        arcs.push_back({G.newEdge(place->node, transition->node), place->output,
                        transition->input});
        GA.bends(arcs.back().edge);
      }

      for (const auto& s : io.second) {
        const auto place =
            std::find_if(places.begin(), places.end(),
                         [&](const auto& n) { return s.first == n.name; });
        arcs.push_back({G.newEdge(transition->node, place->node),
                        transition->output, place->input});
        GA.bends(arcs.back().edge);
      }
    }

    {
      using namespace ogdf;
      for (node v : G.nodes) {
        GA.width(v) /= 2;
        GA.height(v) /= 2;
      }

      GraphIO::write(GA, "output-ERDiagram.svg", GraphIO::drawSVG);
      SugiyamaLayout SL;
      SL.setRanking(new OptimalRanking);
      SL.setCrossMin(new MedianHeuristic);

      OptimalHierarchyLayout* ohl = new OptimalHierarchyLayout;
      ohl->layerDistance(30.0);
      ohl->nodeDistance(15.0);
      ohl->weightBalancing(0.2);
      SL.setLayout(ohl);

      SL.call(GA);
      GA.rotateLeft90();
      GraphIO::write(GA, "output-unix-history-hierarchical.svg",
                     GraphIO::drawSVG);
    }

    double x_factor = 2;
    double y_factor = 3;
    for (auto& t : transitions) {
      t.x = x_factor * GA.x(t.node);
      t.y = y_factor * GA.y(t.node);
    }
    for (auto& p : places) {
      p.x = x_factor * GA.x(p.node);
      p.y = y_factor * GA.y(p.node);
    }
  }
  std::vector<Node> places;
  std::vector<Node> transitions;
  std::vector<Arc> arcs;
};

template <>
void draw(Graph& g) {
  namespace ed = ax::NodeEditor;
  ed::SetCurrentEditor(g.m_Context);
  ed::Begin("My Editor", ImVec2(0.0, 0.0f));
  for (const auto& [name, node, id, i, o, x, y] : g.transitions) {
    ed::BeginNode(id.key());
    ed::BeginPin(i.key(), ed::PinKind::Input);
    ed::EndPin();
    ImGui::SameLine();
    // ImGui::Text("%s", name.c_str());
    // ImGui::SameLine();
    ed::BeginPin(o.key(), ed::PinKind::Output);
    ed::EndPin();
    ed::EndNode();
    ed::SetNodePosition(id.key(), ImVec2(x, y));
  }

  for (const auto& [name, node, id, i, o, x, y] : g.places) {
    ed::BeginNode(id.key());
    ed::BeginPin(i.key(), ed::PinKind::Input);
    ed::EndPin();
    ImGui::SameLine();
    // ImGui::Text("%s", name.c_str());
    // ImGui::SameLine();
    ed::BeginPin(o.key(), ed::PinKind::Output);
    ed::EndPin();
    ed::EndNode();
    ed::SetNodePosition(id.key(), ImVec2(x, y));
  }

  int uniqueId = 1;
  for (const auto& [edge, i, o] : g.arcs) {
    ax::NodeEditor::Link(uniqueId++, i.key(), o.key());
  }
  ed::End();
  ed::SetCurrentEditor(nullptr);
}
