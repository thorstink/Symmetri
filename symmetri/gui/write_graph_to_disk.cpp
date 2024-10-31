#include "write_graph_to_disk.h"

#include "tinyxml2/tinyxml2.h"

namespace farbart {
model::Reducer writeToDisk(const std::filesystem::path& path) {
  return [=](model::Model&& m_ptr) {
    auto& m = *m_ptr.data;
    using namespace tinyxml2;
    XMLDocument doc;
    doc.SaveFile(path.c_str());
    XMLElement* pnml = doc.NewElement("pnml");
    pnml->SetAttribute("xmlns",
                       "http://www.pnml.org/version-2009/grammar/pnml");
    doc.InsertFirstChild(pnml);
    auto net = pnml->InsertFirstChild(doc.NewElement("net"));
    auto root = net->InsertFirstChild(doc.NewElement("page"));

    for (auto idx : m.p_view) {
      XMLElement* child = doc.NewElement("place");
      XMLElement* s_child = doc.NewElement("graphics");
      XMLElement* s_s_child = doc.NewElement("position");
      child->SetAttribute("id", m.net.place[idx].c_str());
      s_s_child->SetAttribute("x", m.p_positions[idx].x);
      s_s_child->SetAttribute("y", m.p_positions[idx].y);
      s_child->InsertEndChild(s_s_child);
      child->InsertEndChild(s_child);
      root->InsertEndChild(child);
    }

    for (auto idx : m.t_view) {
      XMLElement* child = doc.NewElement("transition");
      XMLElement* s_child = doc.NewElement("graphics");
      XMLElement* s_s_child = doc.NewElement("position");
      child->SetAttribute("id", m.net.transition[idx].c_str());
      s_s_child->SetAttribute("x", m.t_positions[idx].x);
      s_s_child->SetAttribute("y", m.t_positions[idx].y);
      s_child->InsertEndChild(s_s_child);
      child->InsertEndChild(s_child);
      root->InsertEndChild(child);

      for (const auto& token : m.net.input_n[idx]) {
        XMLElement* child = doc.NewElement("arc");
        child->SetAttribute("source",
                            m.net.place[std::get<size_t>(token)].c_str());
        child->SetAttribute("target", m.net.transition[idx].c_str());
        child->SetAttribute(
            "color",
            std::string(std::get<symmetri::Token>(token).toString()).c_str());
        root->InsertEndChild(child);
      }
      for (const auto& token : m.net.output_n[idx]) {
        XMLElement* child = doc.NewElement("arc");
        child->SetAttribute("source", m.net.transition[idx].c_str());
        child->SetAttribute("target",
                            m.net.place[std::get<size_t>(token)].c_str());
        child->SetAttribute(
            "color",
            std::string(std::get<symmetri::Token>(token).toString()).c_str());
        root->InsertEndChild(child);
      }
    }

    doc.SaveFile(path.c_str());

    m.active_file = path;
    return m_ptr;
  };
}
}  // namespace farbart
