#pragma once

#include <tuple>

#include "graph.hpp"
#include "imfilebrowser.h"
#include "redux.hpp"
#include "symmetri/parsers.h"

Reducer updateActiveFile(const std::filesystem::path &file) {
  return [=](Model &&m) {
    m.active_file = file;
    auto [net, marking] = symmetri::readPnml({file});
    m.graph->reset(*createGraph(net));
    return m;
  };
}

template <>
void draw(ImGui::FileBrowser &fileDialog) {
  fileDialog.Display();
  if (fileDialog.HasSelected()) {
    MVC::push(updateActiveFile(fileDialog.GetSelected()));
    fileDialog.ClearSelected();
  }
  if (ImGui::BeginMainMenuBar()) {
    // silly but ok.
    MVC::push([menu_height = ImGui::GetWindowSize().y](Model &&m) {
      m.menu_height = menu_height;
      return m;
    });

    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("New")) {
      }
      if (ImGui::MenuItem("Open")) {
        fileDialog.Open();
      }
      // Exit...
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
      //...
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window")) {
      //...
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
      //...
      ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
  }
}
