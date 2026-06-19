#include "draw_about.h"

#include <stddef.h>

#include "imgui.h"
#include "reducers.h"

namespace model {
struct ViewModel;
}  // namespace model

#define QUOTE(x) #x
#define STR(x) QUOTE(x)

void AlignForWidth(float width, float alignment = 0.5f) {
  ImGuiStyle& style = ImGui::GetStyle();
  float avail = ImGui::GetContentRegionAvail().x;
  float off = (avail - width) * alignment;
  if (off > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);
}

// Body of the About popup; the window frame is provided by drawUi.
void draw_about(const model::ViewModel&) {
  ImGui::Text(
      "Farbart version %s\nA GUI for creating and simulating "
      "Symmetri-nets.",
      STR(VERSION));

  ImGuiStyle& style = ImGui::GetStyle();
  float width = 0.0f;
  width += ImGui::CalcTextSize("Ok").x;
  width += style.ItemSpacing.x;
  width += 100.0f;
  width += style.ItemSpacing.x;
  width += ImGui::CalcTextSize("Copy!").x;
  AlignForWidth(width);

  if (ImGui::Button("Ok")) {
    removePopup("About");
  }
  ImGui::SameLine();
  ImGui::InvisibleButton("Fixed", ImVec2(100.0f, 1.0f));  // Fixed size
  ImGui::SameLine();
  if (ImGui::Button("Copy to clipboard")) {
    ImGui::LogToClipboard();
    ImGui::LogText(
        "Farbart version %s\nA GUI for creating and simulating "
        "Symmetri-nets.",
        STR(VERSION));
    ImGui::LogFinish();
  }
}
