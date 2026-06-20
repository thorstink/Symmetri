#include "draw_menu_bar.h"

#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// clang-format off
#include "imgui.h"
#include "imfilebrowser.h"
// clang-format on
#include "draw_about.h"
#include "load_file.h"
#include "path_autocomplete.h"
#include "reducers.h"
#include "rxdispatch.h"
#include "save_file.h"

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
// Example popup body shown when the background task finishes. The window frame
// is provided by drawUi; this fills the body and closes itself by id.
void draw_sleep_popup(const model::ViewModel&) {
  ImGui::TextUnformatted("Background task finished (slept 2s off-thread).");
  if (ImGui::Button("OK")) {
    removePopup("Background task");
  }
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
    farbart::requestSaveGraph(vm);
  }

  static std::once_flag flag;
  std::call_once(flag, [&] {
    file_dialog.SetTitle("Open a Symmetri-net");
    file_dialog.SetTypeFilters({".pnml", ".grml"});
  });
  file_dialog.Display();
  if (file_dialog.HasSelected()) {
    farbart::requestLoad(vm, file_dialog.GetSelected());
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
        farbart::requestSaveGraph(vm);
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
    if (ImGui::BeginMenu("Run")) {
      if (ImGui::MenuItem("Background task (sleep 2s)")) {
        using namespace std::chrono_literals;
        // A Computer runs off the UI thread (on the dispatch thread pool): it
        // sleeps without blocking the view, then yields a view reducer that
        // pops up a dialog. The result Action travels back through the reducer
        // pipeline like any other update.
        rxdispatch::push(model::Computer{[]() -> model::Action {
          std::this_thread::sleep_for(2s);
          // The result is a view reducer that registers the popup as view
          // state (rendered by drawUi from ViewState::popups).
          return model::ViewReducer{
              [](model::ViewState& v, const model::EditState&) {
                v.popups.push_back({"Background task", &draw_sleep_popup});
              }};
        }});
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Window")) {
      //...
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
      if (ImGui::MenuItem("About")) {
        addPopup("About", &draw_about);
      }
      ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
  }
}
