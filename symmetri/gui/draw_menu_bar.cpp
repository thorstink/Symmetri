#include "draw_menu_bar.h"

#include <mutex>
#include <string>
#include <vector>

// clang-format off
#include "imgui.h"
#include "imfilebrowser.h"
// clang-format on
#include "draw_about.h"
#include "load_file.h"
#include "reducers.h"
#include "rxdispatch.h"
#include "write_graph_to_disk.h"

namespace model {
struct ViewModel;
}  // namespace model

ImGui::FileBrowser file_dialog = ImGui::FileBrowser();

void shortcutString(std::string_view text) {
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
  ImGui::Text("%s", text.data());
  ImGui::PopStyleColor();
}
void draw_menu_bar(const model::ViewModel& vm) {
  if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Minus,
                      ImGuiInputFlags_RouteAlways)) {
    zoomRelative(-0.1f);
  }
  if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Equal,
                      ImGuiInputFlags_RouteAlways)) {
    zoomRelative(0.1f);
  }

  if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O,
                      ImGuiInputFlags_RouteAlways)) {
    file_dialog.Open();
  }

  if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S,
                      ImGuiInputFlags_RouteAlways)) {
    rxdispatch::push(farbart::writeGraphToDisk(vm));
  }

  static std::once_flag flag;
  std::call_once(flag, [&] {
    file_dialog.SetTitle("Open a Symmetri-net");
    file_dialog.SetTypeFilters({".pnml", ".grml"});
  });
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
        rxdispatch::push(farbart::writeGraphToDisk(vm));
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
      if (ImGui::MenuItem("Reset view")) {
        resetNetView();
      }

      if (ImGui::MenuItem("Zoom In")) {
        zoomRelative(0.1f);
      }
      shortcutString("cmd +");
      if (ImGui::MenuItem("Zoom Out")) {
        zoomRelative(-0.1f);
      }
      shortcutString("cmd -");

      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Window")) {
      //...
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
      if (ImGui::MenuItem("About")) {
        addView(&draw_about);
      }
      ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
  }
}
