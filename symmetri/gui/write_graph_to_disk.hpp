#pragma once

#include <filesystem>

#include "model.h"
#include "tinyxml2/tinyxml2.h"

namespace farbart {
inline model::Reducer writeToDisk(const std::filesystem::path& path) {
  return [=](model::Model& m_ptr) {
    auto& m = *m_ptr.data;
    const auto& graph = m.graph;
    using namespace tinyxml2;
    XMLDocument doc;
    doc.SaveFile(path.c_str());
    XMLElement* pnml = doc.NewElement("pnml");
    pnml->SetAttribute("xmlns",
                       "http://www.pnml.org/version-2009/grammar/pnml");
    doc.InsertFirstChild(pnml);
    auto net = pnml->InsertFirstChild(doc.NewElement("net"));
    auto root = net->InsertFirstChild(doc.NewElement("page"));
    const auto& nodes = graph.nodes;
    for (auto idx : graph.n_idx) {
      const auto& node = nodes[idx];
      XMLElement* child = doc.NewElement(
          node.type == Node::Type::Place ? "place" : "transition");
      XMLElement* s_child = doc.NewElement("graphics");
      XMLElement* s_s_child = doc.NewElement("position");
      child->SetAttribute("id", node.name.c_str());
      s_s_child->SetAttribute("x", node.Pos.x);
      s_s_child->SetAttribute("y", node.Pos.y);
      s_child->InsertEndChild(s_s_child);
      child->InsertEndChild(s_child);
      root->InsertEndChild(child);
    }
    const auto& arcs = graph.arcs;

    for (auto idx : graph.a_idx) {
      const auto& arc = arcs[idx];
      XMLElement* child = doc.NewElement("arc");
      child->SetAttribute("source", nodes[arc.from_to_pos_idx[0]].name.c_str());
      child->SetAttribute("target", nodes[arc.from_to_pos_idx[1]].name.c_str());
      child->SetAttribute("color",
                          symmetri::Color::toString(arc.color).c_str());

      root->InsertEndChild(child);
    }

    doc.SaveFile(path.c_str());

    m.working_dir = path.parent_path();
    m.active_file = path;
    return m_ptr;
  };
}
}  // namespace farbart
