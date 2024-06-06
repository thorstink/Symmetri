#define IMGUI_DEFINE_MATH_OPERATORS

#include "draw_menu.h"

#include "graph_reducers.h"
#include "imgui_internal.h"

void draw_menu(const model::ViewModel& vm) {
  // is now also true if there's nothing selected.
  const bool is_a_node_selected = vm.selected_node_idx.has_value();
  const bool is_place = vm.selected_node_idx.has_value() &&
                        std::get<0>(vm.selected_node_idx.value());
  const size_t selected_idx =
      is_a_node_selected ? std::get<1>(vm.selected_node_idx.value()) : 9999;

  ImVec2 WindowSize = ImGui::GetWindowSize();
  WindowSize.y -= 140.0f;
  // Draw a list of nodes on the left side
  ImGui::BeginChild("some", ImVec2(200, 0));
  ImGui::Text("Selected");
  ImGui::Separator();
  ImGui::BeginChild("selected_node", ImVec2(200, 0.1 * WindowSize.y));

  if (vm.selected_node_idx.has_value()) {
    ImGui::Text("Name");
    ImGui::SameLine();
    static int i = 0;
    const auto id = std::string("##") + std::to_string(i++);
    ImGui::PushItemWidth(-1);
    const auto& model_name =
        (is_place ? vm.net.place : vm.net.transition)[selected_idx];
    ImGui::Text("%s", model_name.c_str());
    static const char* data = nullptr;
    static char view_name[128] = "";
    static std::optional<std::tuple<bool, size_t>> local_idx;
    if (local_idx.has_value() &&
        vm.selected_node_idx.value() != local_idx.value()) {
      data = model_name.data();
      local_idx = vm.selected_node_idx;
    } else if (data != model_name.data()) {
      data = model_name.data();
      strcpy(view_name, model_name.c_str());
    } else if (std::string_view(view_name).compare(model_name)) {
      // check if name is correct correct...
      is_place ? updatePlaceName(selected_idx, std::string(view_name))
               : updateTransitionName(selected_idx, std::string(view_name));
    }

    ImGui::InputText("input text", view_name, 128);
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

    static std::optional<
        std::pair<std::tuple<bool, size_t, size_t>, symmetri::Token>>
        local_color = std::nullopt;
    if (not local_color.has_value()) {
      local_color = {vm.selected_arc_idxs.value(), color};
    } else if (vm.selected_arc_idxs.value() == local_color->first &&
               local_color->second != color) {
      updateArcColor(is_input, selected_idx, sub_idx, local_color->second);
    } else if (vm.selected_arc_idxs.value() != local_color->first) {
      local_color = {vm.selected_arc_idxs.value(), color};
    }

    if (ImGui::BeginMenu(symmetri::Color::toString(color).c_str())) {
      for (const auto& color : vm.colors) {
        if (ImGui::MenuItem(color.c_str())) {
          local_color = {vm.selected_arc_idxs.value(),
                         symmetri::Color::registerToken(color)};
        }
      }
      ImGui::EndMenu();
    }
  }
  ImGui::EndChild();

  ImGui::Dummy(ImVec2(0.0f, 20.0f));

  ImGui::Text("Places");
  ImGui::Separator();
  constexpr float height_fraction = 0.8 / 2.0;
  ImGui::BeginChild("place_list", ImVec2(200, height_fraction * WindowSize.y));

  for (const auto& idx : vm.p_view) {
    renderNodeEntry(true, vm.net.place[idx], idx,
                    is_a_node_selected && is_place && idx == selected_idx);
  }
  ImGui::EndChild();
  ImGui::Dummy(ImVec2(0.0f, 20.0f));
  ImGui::Text("Transitions");
  ImGui::Separator();
  ImGui::BeginChild("transition_list",
                    ImVec2(200, height_fraction * WindowSize.y));
  for (const auto& idx : vm.t_view) {
    renderNodeEntry(false, vm.net.transition[idx], idx,
                    is_a_node_selected && !is_place && idx == selected_idx);
  }

  ImGui::EndChild();
  ImGui::EndChild();
}
