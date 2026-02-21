#define IMGUI_DEFINE_MATH_OPERATORS

#include "draw_menu.h"

#include <ranges>

#include "imgui_internal.h"
#include "reducers.h"
#include "shared.h"
#include "simulation_menu.h"

static size_t label_id = 5;
void drawTokenLine(const symmetri::AugmentedToken& at) {
  ImGui::PushID(++label_id);
  ImGui::PushStyleColor(ImGuiCol_Button,
                        (ImVec4)ImColor::HSV(0 / 7.0f, 0.6f, 0.6f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        (ImVec4)ImColor::HSV(0 / 7.0f, 0.7f, 0.7f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        (ImVec4)ImColor::HSV(0 / 7.0f, 0.8f, 0.8f));
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0, 1.0, 1.0, 1.0));

  if (ImGui::Button("-")) {
    removeTokenFromPlace(at);
  };
  ImGui::PopStyleColor(4);
  ImGui::PopID();
  ImGui::SameLine();
  ImGui::TextColored(
      ImGui::ColorConvertU32ToFloat4(getColor(std::get<symmetri::Token>(at))),
      "%s", std::get<symmetri::Token>(at).toString().data());
}

void draw_menu(const model::ViewModel& vm) {
  // is now also true if there's nothing selected.
  const bool is_a_node_selected = vm.selected_node_idx.has_value();
  const model::Model::NodeType node_type =
      std::get<0>(vm.selected_node_idx.value_or(
          std::make_tuple(model::Model::NodeType::Place, size_t(0))));

  const size_t selected_idx =
      is_a_node_selected ? std::get<1>(vm.selected_node_idx.value()) : 9999;
  draw_simulation_menu(vm);

  ImGui::Text("Selected");
  ImGui::Separator();
  if (vm.selected_node_idx.has_value()) {
    ImGui::Text("Name");
    ImGui::SameLine();
    static int i = 0;
    const auto id = std::string("##") + std::to_string(i++);
    ImGui::PushItemWidth(-1);
    const auto& model_name = (model::Model::NodeType::Place == node_type
                                  ? vm.net.place
                                  : vm.net.transition)[selected_idx];
    ImGui::Text("%s", model_name.c_str());

    static char view_name[128] = "";
    strcpy(view_name, model_name.c_str());
    ImGui::PushItemWidth(-1);
    ImGui::InputText("##input tex", view_name, 128,
                     ImGuiInputTextFlags_CallbackEdit,
                     (model::Model::NodeType::Place == node_type
                          ? updatePlaceName
                          : updateTransitionName)(selected_idx));
    ImGui::PopItemWidth();
    label_id = 0;
    if (model::Model::NodeType::Place == node_type) {
      ImGui::Text("Marking");
      std::ranges::for_each(vm.tokens | std::views::filter([=](const auto& at) {
                              return std::get<size_t>(at) == selected_idx;
                            }),
                            &drawTokenLine);
    } else if (model::Model::NodeType::Place != node_type) {
      ImGui::Text("Priority");
      ImGui::SameLine();
      static char view_priority[4] = "";
      strcpy(view_priority,
             std::to_string(vm.net.priority[selected_idx]).c_str());
      ImGui::InputText("##priority", view_priority, 4,
                       ImGuiInputTextFlags_CharsDecimal |
                           ImGuiInputTextFlags_CharsNoBlank |
                           ImGuiInputTextFlags_CallbackEdit,
                       updatePriority(selected_idx));
      ImGui::Text("Output");
      ImGui::SameLine();
      if (ImGui::BeginCombo(
              "##output", fire(vm.net.store[selected_idx]).toString().data())) {
        for (const auto& color : vm.colors) {
          if (ImGui::Selectable(color.data())) {
            updateTransitionOutputColor(selected_idx,
                                        symmetri::Token(color.data()));
          }
        }
        ImGui::EndCombo();
      }
    }
  } else if (vm.selected_arc_idxs.has_value()) {
    const auto [is_input, selected_idx, sub_idx] = vm.selected_arc_idxs.value();

    const auto color = std::get<symmetri::Token>(
        (is_input ? vm.net.input_n : vm.net.output_n)[selected_idx][sub_idx]);

    if (is_input) {
      drawColorDropdownMenu(std::string(color.toString()), vm.colors,
                            [=](auto c) {
                              updateArcColor(is_input, selected_idx, sub_idx,
                                             symmetri::Token(c.data()));
                            });
    } else {
      ImGui::Text("%s", color.toString().data());
    }
  }
  ImGui::Dummy(ImVec2(0.0f, 20.0f));

  ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
  if (ImGui::BeginTabBar("SymmetriTabBar", tab_bar_flags)) {
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
        renderNodeEntry(model::Model::NodeType::Place, vm.net.place[idx], idx,
                        is_a_node_selected &&
                            model::Model::NodeType::Place == node_type &&
                            idx == selected_idx);
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
        renderNodeEntry(
            model::Model::NodeType::Transition, vm.net.transition[idx], idx,
            is_a_node_selected && model::Model::NodeType::Place != node_type &&
                idx == selected_idx);
      }

      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Marking")) {
      for (const auto& [place, color] : vm.tokens) {
        ImGui::Text("%s,", vm.net.place[place].c_str());
        ImGui::SameLine();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(getColor(color)),
                           "%s", color.toString().data());
      }
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Colors")) {
      static char new_color[128] = "";
      ImGui::PushItemWidth(-79);
      ImGui::InputText("##4", new_color, 128, ImGuiInputTextFlags_CharsNoBlank);
      ImGui::SameLine();
      if (ImGui::Button("Add Color")) {
        symmetri::Token(const_cast<const char*>(new_color));
        memset(new_color, 0, sizeof(new_color));
        updateColorTable();
      }
      ImGui::Separator();
      for (const auto& color : vm.colors) {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(
                               getColor(symmetri::Token(color.data()))),
                           "%s", color.data());
      }
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
}
