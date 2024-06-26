#define IMGUI_DEFINE_MATH_OPERATORS

#include "draw_ui.h"

#include "draw_context_menu.h"
#include "draw_graph.h"
#include "draw_menu.h"
#include "imgui.h"
#include "menu_bar.h"
#include "reducers.h"

constexpr float menu_width = 350;

void draw_everything(const model::ViewModel& vm) {
  draw_menu_bar(vm);
  const auto no_move_draw_resize = ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoTitleBar |
                                   ImGuiWindowFlags_NoMove;
  ImGui::Begin("main", NULL, no_move_draw_resize);
  ImGui::SetNextWindowSize(
      ImVec2(menu_width, ImGui::GetIO().DisplaySize.y - 20));
  ImGui::SetNextWindowPos(ImVec2(0, 20));

  ImGui::Begin("tools", NULL, no_move_draw_resize);
  draw_menu(vm);
  ImGui::End();
  ImGui::SameLine();
  ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x - menu_width,
                                  ImGui::GetIO().DisplaySize.y));
  ImGui::SetNextWindowPos(ImVec2(menu_width, 20));
  ImGui::Begin("graph", NULL, no_move_draw_resize);

  if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) &&
      ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
    setContextMenuActive();
  } else if (vm.context_menu_active &&
             ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
             !ImGui::IsAnyItemHovered()) {
    setContextMenuInactive();
  }

  draw_graph(vm);
  ImGui::End();
  ImGui::End();
}
