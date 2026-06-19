#include "save_file.h"

#include <stddef.h>

#include <algorithm>
#include <filesystem>
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "gui/model.h"
#include "gui/rxdispatch.h"
#include "imgui.h"
#include "petri.h"
#include "reducers.h"
#include "small_vector.hpp"
#include "symmetri/colors.hpp"
#include "tinyxml2/tinyxml2.h"

namespace farbart {

auto toXml(const symmetri::Petri::PTNet& net,
           const std::vector<model::Coordinate>& p_positions,
           const std::vector<model::Coordinate>& t_positions,
           const std::vector<symmetri::AugmentedToken>& tokens) {
  using namespace tinyxml2;
  auto doc = std::make_unique<XMLDocument>();
  XMLElement* pnml = doc->NewElement("pnml");
  pnml->SetAttribute("xmlns", "http://www.pnml.org/version-2009/grammar/pnml");
  doc->InsertFirstChild(pnml);
  auto xmlnet = pnml->InsertFirstChild(doc->NewElement("net"));
  auto root = xmlnet->InsertFirstChild(doc->NewElement("page"));

  for (size_t idx = 0; idx < net.place.size(); ++idx) {
    XMLElement* child = doc->NewElement("place");
    XMLElement* s_child = doc->NewElement("graphics");
    XMLElement* s_s_child = doc->NewElement("position");
    child->SetAttribute("id", net.place[idx].c_str());
    s_s_child->SetAttribute("x", p_positions[idx].x);
    s_s_child->SetAttribute("y", p_positions[idx].y);
    s_child->InsertEndChild(s_s_child);
    child->InsertEndChild(s_child);
    for (const auto& token : tokens) {
      if (std::get<size_t>(token) == idx) {
        XMLElement* marking = doc->NewElement("initialMarking");
        marking->SetAttribute(
            "color",
            std::string(std::get<symmetri::Token>(token).toString()).c_str());
        child->InsertEndChild(marking);
      }
    }
    root->InsertEndChild(child);
  }

  for (size_t idx = 0; idx < net.transition.size(); ++idx) {
    XMLElement* child = doc->NewElement("transition");
    XMLElement* s_child = doc->NewElement("graphics");
    XMLElement* s_s_child = doc->NewElement("position");
    child->SetAttribute("id", net.transition[idx].c_str());
    s_s_child->SetAttribute("x", t_positions[idx].x);
    s_s_child->SetAttribute("y", t_positions[idx].y);
    s_child->InsertEndChild(s_s_child);
    child->InsertEndChild(s_child);
    root->InsertEndChild(child);

    for (const auto& token : net.input_n[idx]) {
      XMLElement* child = doc->NewElement("arc");
      child->SetAttribute("source", net.place[std::get<size_t>(token)].c_str());
      child->SetAttribute("target", net.transition[idx].c_str());
      child->SetAttribute(
          "color",
          std::string(std::get<symmetri::Token>(token).toString()).c_str());
      root->InsertEndChild(child);
    }
    for (const auto& token : net.output_n[idx]) {
      XMLElement* child = doc->NewElement("arc");
      child->SetAttribute("source", net.transition[idx].c_str());
      child->SetAttribute("target", net.place[std::get<size_t>(token)].c_str());
      child->SetAttribute(
          "color",
          std::string(std::get<symmetri::Token>(token).toString()).c_str());
      root->InsertEndChild(child);
    }
  }
  return doc;
}

void requestSaveGraph(const model::ViewModel& vm) {
  const std::string path = vm.active_file;
  if (std::filesystem::path(path).extension() != ".pnml") {
    return;
  }
  // Serialize on the calling (UI) thread, where the net is not being mutated
  // concurrently (capturing the live net into the background task would race
  // with edit reducers). The finished, owned document is then all the popup /
  // background write needs. toXml yields a unique_ptr that can't live in a
  // std::function, so share it.
  std::shared_ptr<tinyxml2::XMLDocument> doc =
      toXml(vm.net, vm.p_positions, vm.t_positions, vm.tokens);

  const std::string id = "Save graph";
  addPopup(id, [doc, path, id](const model::ViewModel&) {
    ImGui::TextUnformatted("Save the net to:");
    ImGui::TextWrapped("%s", path.c_str());
    ImGui::SetNextItemShortcut(ImGuiKey_Enter);
    if (ImGui::Button("Save")) {
      // The actual write happens off the UI thread; nothing blocks here.
      rxdispatch::push(model::Computer{[doc, path]() -> model::Action {
        doc->SaveFile(path.c_str());
        return model::EditReducer{[path](model::EditState&& e) {
          e.active_file = path.c_str();
          return e;
        }};
      }});
      removePopup(id);
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      removePopup(id);
    }
  });
}

}  // namespace farbart
