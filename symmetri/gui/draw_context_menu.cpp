#define IMGUI_DEFINE_MATH_OPERATORS

#include "draw_menu.h"
#include "imgui_internal.h"
#include "reducers.h"
#include "shared.h"
#include "simulation_menu.h"

void draw_context_menu(const model::ViewModel& vm) {
  ImGui::OpenPopup("context_menu");
  // Draw context menu
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
  if (ImGui::BeginPopup("context_menu")) {
    if (vm.selected_node_idx.has_value()) {
      const model::Model::NodeType node_type =
          std::get<0>(vm.selected_node_idx.value());
      const size_t selected_idx = std::get<1>(vm.selected_node_idx.value());
      ImGui::Text(
          "%s '%s'",
          (node_type == model::Model::NodeType::Place ? "Place" : "Transition"),
          (node_type == model::Model::NodeType::Place
               ? vm.net.place
               : vm.net.transition)[selected_idx]
              .c_str());
      ImGui::Separator();
      if (ImGui::BeginMenu("Add arc to...")) {
        for (const auto& node_idx : node_type == model::Model::NodeType::Place
                                        ? vm.t_view
                                        : vm.p_view) {
          if (node_type == model::Model::NodeType::Place) {
            drawColorDropdownMenu(vm.net.transition[node_idx], vm.colors,
                                  [&](auto c) -> void {
                                    addArc(node_type, selected_idx, node_idx,
                                           symmetri::Token(c.data()));
                                  });

          } else if (ImGui::MenuItem((vm.net.place[node_idx].c_str()))) {
            addArc(node_type, selected_idx, node_idx, symmetri::Success);
          }
          if (ImGui::IsItemHovered()) {
            const auto target_node_type =
                node_type == model::Model::NodeType::Place
                    ? model::Model::NodeType::Transition
                    : model::Model::NodeType::Place;
            if (not vm.selected_target_node_idx.has_value() ||
                vm.selected_target_node_idx.value() !=
                    std::tuple<model::Model::NodeType, size_t>{target_node_type,
                                                               node_idx}) {
              setSelectedTargetNode(target_node_type, node_idx);
            }
          }
        }
        ImGui::EndMenu();
      }
      if (node_type == model::Model::NodeType::Place) {
        drawColorDropdownMenu("Add marking", vm.colors, [=](auto c) -> void {
          addTokenToPlace(symmetri::AugmentedToken{selected_idx,
                                                   symmetri::Token(c.data())});
        });
      }

      if (ImGui::MenuItem("Delete")) {
        node_type == model::Model::NodeType::Place
            ? removePlace(selected_idx)
            : removeTransition(selected_idx);
      }
    } else if (vm.selected_arc_idxs.has_value()) {
      ImGui::Text("Arc");
      ImGui::Separator();
      if (ImGui::MenuItem("Delete")) {
        const auto& [source_node_type, idx, sub_idx] =
            vm.selected_arc_idxs.value();
        removeArc(source_node_type, idx, sub_idx);
      }
    } else {
      // @todo make this offset relative to the menubars
      ImVec2 scene_pos = ImGui::GetIO().MousePos + ImVec2(-275, -40);
      if (ImGui::MenuItem("Add place")) {
        addNode(model::Model::NodeType::Place, scene_pos);
      }
      if (ImGui::MenuItem("Add transition")) {
        addNode(model::Model::NodeType::Transition, scene_pos);
      }
    }
    ImGui::EndPopup();
  }
  ImGui::PopStyleVar();
}
