#define IMGUI_DEFINE_MATH_OPERATORS

#include "draw_menu.h"
#include "graph_reducers.h"
#include "imgui_internal.h"

void drawContextMenu(const model::ViewModel& vm) {
  // Open context menu
  if (not vm.context_menu_active &&
      ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) ||
        !ImGui::IsAnyItemHovered()) {
      setContextMenuActive();
    }
  }

  if (vm.context_menu_active) {
    ImGui::OpenPopup("context_menu");
  }

  // Draw context menu
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
  if (ImGui::BeginPopup("context_menu")) {
    if (vm.selected_node_idx.has_value()) {
      const bool is_place = std::get<0>(vm.selected_node_idx.value());
      const size_t selected_idx = std::get<1>(vm.selected_node_idx.value());
      ImGui::Text(
          "Node '%s'",
          (is_place ? vm.net.place : vm.net.transition)[selected_idx].c_str());
      ImGui::Separator();
      if (ImGui::BeginMenu("Add arc to...")) {
        for (const auto& node_idx : is_place ? vm.t_view : vm.p_view) {
          if (is_place) {
            if (ImGui::BeginMenu(vm.net.transition[node_idx].c_str())) {
              for (const auto& color : vm.colors) {
                if (ImGui::MenuItem(color.c_str())) {
                  addArc(is_place, selected_idx, node_idx,
                         symmetri::Color::registerToken(color));
                }
              }
              ImGui::EndMenu();
            }
          } else {
            if (ImGui::MenuItem((vm.net.place[node_idx].c_str()))) {
              addArc(is_place, selected_idx, node_idx,
                     symmetri::Color::Success);
            }
          }
        }
        ImGui::EndMenu();
      }

      if (ImGui::MenuItem("Delete")) {
        is_place ? removePlace(selected_idx) : removeTransition(selected_idx);
      }
    } else if (vm.selected_arc_idxs.has_value()) {
      ImGui::Text("Arc");
      ImGui::Separator();
      if (ImGui::MenuItem("Delete")) {
        const auto& [is_input, idx, sub_idx] = vm.selected_arc_idxs.value();
        removeArc(is_input, idx, sub_idx);
      }
    } else {
      ImVec2 scene_pos = ImGui::GetMousePosOnOpeningCurrentPopup() -
                         (ImGui::GetCursorScreenPos() + vm.scrolling);
      if (ImGui::MenuItem("Add place")) {
        addNode(true, scene_pos);
      }
      if (ImGui::MenuItem("Add transition")) {
        addNode(false, scene_pos);
      }
    }
    ImGui::EndPopup();
  }
  ImGui::PopStyleVar();
  // Scrolling
  if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive() &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f)) {
    moveView(ImGui::GetIO().MouseDelta);
    if (vm.context_menu_active) {
      setContextMenuInactive();
    }
  }

  ImGui::PopItemWidth();
  ImGui::EndChild();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();
  ImGui::EndGroup();
}
