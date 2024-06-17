#define IMGUI_DEFINE_MATH_OPERATORS

#include "draw_menu.h"

#include "graph_reducers.h"
#include "imgui_internal.h"
#include "shared.h"

void draw_menu(const model::ViewModel& vm) {
  // is now also true if there's nothing selected.
  const bool is_a_node_selected = vm.selected_node_idx.has_value();
  const bool is_place = vm.selected_node_idx.has_value() &&
                        std::get<0>(vm.selected_node_idx.value());
  const size_t selected_idx =
      is_a_node_selected ? std::get<1>(vm.selected_node_idx.value()) : 9999;

  ImGui::Text("Selected");
  ImGui::Separator();
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
    ImGui::InputText("##input tex", view_name, 128,
                     ImGuiInputTextFlags_CallbackEdit,
                     is_place ? updatePlaceName(selected_idx)
                              : updateTransitionName(selected_idx));
    ImGui::PopItemWidth();

    if (not is_place) {
      ImGui::Text("Priority");
      ImGui::SameLine();
      static char view_priority[4] = "";
      strcpy(view_priority,
             std::to_string(vm.net.priority[selected_idx]).c_str());
      ImGui::InputText("##", view_priority, 4,
                       ImGuiInputTextFlags_CharsDecimal |
                           ImGuiInputTextFlags_CharsNoBlank |
                           ImGuiInputTextFlags_CallbackEdit,
                       updatePriority(selected_idx));
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

  ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
  if (ImGui::BeginTabBar("MyTabBar", tab_bar_flags)) {
    if (ImGui::BeginTabItem("Places")) {
      static char new_place[128] = "Not yet available...";
      ImGui::PushItemWidth(-37);
      ImGui::InputText("##2", new_place, 128, ImGuiInputTextFlags_CharsNoBlank);
      ImGui::SameLine();
      if (ImGui::Button("Add")) {
        memset(new_place, 0, sizeof(new_place));
      }
      ImGui::Separator();

      for (const auto& idx : vm.p_view) {
        renderNodeEntry(true, vm.net.place[idx], idx,
                        is_a_node_selected && is_place && idx == selected_idx);
      }
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Transitions")) {
      static char new_transition[128] = "Not yet available...";
      ImGui::PushItemWidth(-37);
      ImGui::InputText("##3", new_transition, 128,
                       ImGuiInputTextFlags_CharsNoBlank);
      ImGui::SameLine();
      if (ImGui::Button("Add")) {
        memset(new_transition, 0, sizeof(new_transition));
      }
      ImGui::Separator();
      for (const auto& idx : vm.t_view) {
        renderNodeEntry(false, vm.net.transition[idx], idx,
                        is_a_node_selected && !is_place && idx == selected_idx);
      }

      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Marking")) {
      for (const auto& [place, color] : vm.tokens) {
        ImGui::Text("(%s,", vm.net.place[place].c_str());
        ImGui::SameLine();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(getColor(color)),
                           "%s", symmetri::Color::toString(color).c_str());
        ImGui::SameLine();
        ImGui::Text(")");
      }
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Colors")) {
      static char new_color[128] = "";
      ImGui::PushItemWidth(-79);
      ImGui::InputText("##4", new_color, 128, ImGuiInputTextFlags_CharsNoBlank);
      ImGui::SameLine();
      if (ImGui::Button("Add Color")) {
        symmetri::Color::registerToken(std::string(new_color));
        memset(new_color, 0, sizeof(new_color));
        updateColorTable();
      }
      ImGui::Separator();
      for (const auto& color : vm.colors) {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(
                               getColor(symmetri::Color::registerToken(color))),
                           "%s", color.c_str());
      }
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
}
