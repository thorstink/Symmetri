#pragma once

#include <tuple>

#include "graph.hpp"
#include "marking.hpp"
#include "menu_bar.hpp"
#include "view.hpp"
inline Model initializeModel(Model &&m) {
  m.working_dir = "/users/thomashorstink/Projects/symmetri/nets";
  // m.active_file = "/users/thomashorstink/Projects/symmetri/nets/n1.pnml";
  m.active_file =
      "/Users/thomashorstink/Projects/Symmetri/examples/combinations/"
      "DualProcessWorker.pnml";
  symmetri::Net net;
  symmetri::Marking marking;
  symmetri::PriorityTable pt;
  const std::filesystem::path pn_file = m.active_file.value();
  if (pn_file.extension() == std::string(".pnml")) {
    std::tie(net, marking) = symmetri::readPnml({pn_file});
  } else {
    std::tie(net, marking, pt) = symmetri::readGrml({pn_file});
  }

  m.graph = std::make_shared<Graph>();
  m.graph->reset(*createGraph(net));

  // create a file browser instance
  ImGui::FileBrowser fileDialog;

  // (optional) set browser properties
  fileDialog.SetTitle("title");
  fileDialog.SetTypeFilters({".pnml", ".grml"});
  fileDialog.SetPwd(m.working_dir);
  m.statics.push_back(fileDialog);
  return m;
}
