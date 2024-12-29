#include "draw_popups.h"

#include "reducers.h"

const auto no_move_draw_resize =
    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar |
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize;

void draw_confirmation_popup(const model::ViewModel&) {
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  ImGui::Begin("About", NULL, no_move_draw_resize);
  ImGui::Text("Are you sure you want to save to ");

  if (ImGui::Button("Ok")) {
    removeView(&draw_confirmation_popup);
  }
  ImGui::End();
}
