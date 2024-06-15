#define IMGUI_DEFINE_MATH_OPERATORS

#include "draw_menu.h"

#include "graph_reducers.h"
#include "imgui_internal.h"
#include "shared.h"

void draw_menu(const model::ViewModel& vm, float width) {
  // is now also true if there's nothing selected.
  const bool is_a_node_selected = vm.selected_node_idx.has_value();
  const bool is_place = vm.selected_node_idx.has_value() &&
                        std::get<0>(vm.selected_node_idx.value());
  const size_t selected_idx =
      is_a_node_selected ? std::get<1>(vm.selected_node_idx.value()) : 9999;

  ImGui::Text("Selected");
  ImGui::Separator();
  const float selected_node_height = 100.0;
  ImGui::BeginChild("selected_node", ImVec2(0.9 * width, selected_node_height));

  if (vm.selected_node_idx.has_value()) {
    ImGui::Text("Name");
    ImGui::SameLine();
    static int i = 0;
    const auto id = std::string("##") + std::to_string(i++);
    ImGui::PushItemWidth(-1);
    const auto& model_name =
        (is_place ? vm.net.place : vm.net.transition)[selected_idx];
    ImGui::Text("%s", model_name.c_str());

    static char view_name[128] = "";
    strcpy(view_name, model_name.c_str());
    ImGui::PushItemWidth(-1);
    ImGui::InputText("input tex", view_name, 128,
                     ImGuiInputTextFlags_CallbackEdit,
                     is_place ? updatePlaceName(selected_idx)
                              : updateTransitionName(selected_idx));

    ImGui::PopItemWidth();
    static std::optional<std::pair<size_t, int>> local_priority = std::nullopt;
    if (is_a_node_selected && not is_place) {
      if (not local_priority.has_value()) {
        local_priority = std::make_pair(
            selected_idx, static_cast<int>(vm.net.priority[selected_idx]));
      } else if (selected_idx == local_priority->first &&
                 local_priority->second != vm.net.priority[selected_idx]) {
        updateTransitionPriority(selected_idx, local_priority->second);
      } else if (selected_idx != local_priority->first) {
        local_priority.reset();
      }

      if (not is_place && local_priority.has_value()) {
        ImGui::Text("Priority");
        ImGui::SameLine();
        ImGui::InputInt("##", &(local_priority->second));
      }
    }
  } else if (vm.selected_arc_idxs.has_value()) {
    const auto& [is_input, selected_idx, sub_idx] =
        vm.selected_arc_idxs.value();

    const auto color =
        (is_input ? vm.net.input_n : vm.net.output_n)[selected_idx][sub_idx]
            .color;

    if (is_input) {
      drawColorDropdownMenu(symmetri::Color::toString(color), vm.colors,
                            [=](const std::string& c) {
                              updateArcColor(is_input, selected_idx, sub_idx,
                                             symmetri::Color::registerToken(c));
                            });
    } else {
      ImGui::Text("%s", symmetri::Color::toString(color).c_str());
    }
  }
  ImGui::EndChild();
  const float rest_height =
      0.4f * (ImGui::GetIO().DisplaySize.y - selected_node_height);
  ImGui::Text("Places");
  ImGui::Separator();
  ImGui::BeginChild("place_list", ImVec2(0.9 * width, rest_height));
  for (const auto& idx : vm.p_view) {
    renderNodeEntry(true, vm.net.place[idx], idx,
                    is_a_node_selected && is_place && idx == selected_idx);
  }
  ImGui::EndChild();
  ImGui::Text("Transitions");
  ImGui::Separator();
  ImGui::BeginChild("transition_list", ImVec2(0.9 * width, rest_height));
  for (const auto& idx : vm.t_view) {
    renderNodeEntry(false, vm.net.transition[idx], idx,
                    is_a_node_selected && !is_place && idx == selected_idx);
  }

  ImGui::EndChild();
}
