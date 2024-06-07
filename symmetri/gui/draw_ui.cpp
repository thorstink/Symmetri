#define IMGUI_DEFINE_MATH_OPERATORS

#include "draw_ui.h"

#include "draw_context_menu.h"
#include "draw_graph.h"
#include "draw_menu.h"
#include "graph_reducers.h"
#include "imgui.h"
#include "menu_bar.h"

void draw_everything(const model::ViewModel& vm) {
  draw_menu_bar(vm);
  ImGui::Begin("test", NULL, ImGuiWindowFlags_NoTitleBar);

  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
      !ImGui::IsAnyItemHovered()) {
    setContextMenuInactive();
    if (vm.selected_node_idx.has_value() || vm.selected_arc_idxs.has_value()) {
      resetSelection();
    }
  }

  draw_menu(vm);
  ImGui::SameLine();
  ImGui::BeginGroup();
  draw_graph(vm);
  draw_context_menu(vm);
  ImGui::EndGroup();
  ImGui::End();
}
