#include "menu_bar.h"

#include "load_file.h"
#include "rxdispatch.h"
#include "symmetri/parsers.h"
#include "write_graph_to_disk.hpp"

void draw_menu_bar(const model::ViewModel &vm) {
  auto &file_dialog = model::ViewModel::file_dialog;
  file_dialog.Display();
  if (file_dialog.HasSelected()) {
    loadPetriNet(file_dialog.GetSelected());
    file_dialog.ClearSelected();
  }
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("New")) {
      }
      if (ImGui::MenuItem("Open")) {
        file_dialog.Open();
      }
      if (ImGui::MenuItem("Save")) {
        rxdispatch::push(farbart::writeToDisk(std::filesystem::path(
            "/Users/thomashorstink/Projects/Symmetri/nets/test9001.pnml")));
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
  ImGui::SetNextWindowSize(
      ImVec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y));
  ImGui::SetNextWindowPos(ImVec2(0, 20));  // fix this
}
