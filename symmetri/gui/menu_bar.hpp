#pragma once

#include <tuple>

#include "imfilebrowser.h"
#include "redux.hpp"
#include "symmetri/parsers.h"

std::function<Model(Model &&)> updateActiveFile(
    const std::filesystem::path &file) {
  return [=](Model &&m) {
    m.active_file = file;
    Net net;
    std::tie(net, m.marking) = symmetri::readPnml({file});
    m.drawables.push_back(net);
    return m;
  };
}

void menuBar(ImGui::FileBrowser &fileDialog) {
  fileDialog.Display();
  if (fileDialog.HasSelected()) {
    MVC::push(updateActiveFile(fileDialog.GetSelected()));
    fileDialog.ClearSelected();
  }
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("New")) {
      }
      if (ImGui::MenuItem("Open", "Ctrl+O")) {
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

void draw(const ImGui::FileBrowser &file_dialog) { menuBar(file_dialog); }
