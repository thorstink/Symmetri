#define IMGUI_DEFINE_MATH_OPERATORS

#include "draw_ui.h"

#include "draw_context_menu.h"
#include "draw_graph.h"
#include "draw_menu.h"
#include "graph_reducers.h"
#include "imgui.h"

void draw_everything(const model::ViewModel& vm) {
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
}
