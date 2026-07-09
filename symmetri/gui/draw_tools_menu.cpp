#define IMGUI_DEFINE_MATH_OPERATORS

#include "draw_tools_menu.h"

#include <stddef.h>

#include <algorithm>
#include <functional>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "gui/model.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "petri.h"
#include "reducers.h"
#include "shared.h"
#include "simulation_menu.h"
#include "symmetri/callback.h"
#include "symmetri/colors.hpp"

void drawTokenLine(const symmetri::AugmentedToken& at, int id) {
  ImGui::PushID(id);
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
  ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(getColor(at.color)), "%s",
                     at.color.toString().data());
}

void draw_selection_menu(const model::ViewModel& vm) {
  ImGui::Text("Selected");
  if (not vm.p_highlight.empty() ||
      (vm.p_highlight.empty() && vm.t_highlight.empty() &&
       vm.arc_highlight.empty()))
    ImGui::Separator();
  static std::map<size_t, std::string> names;  // in-place buffer for changes.
  for (auto idx : vm.p_highlight) {
    ImGui::PushID(idx);
    ImGui::Text("Place");
    ImGui::SameLine();
    ImGui::PushItemWidth(-5);
    const auto [l, inserted] = names.try_emplace(idx, vm.net.place[idx]);
    ImGui::InputText("##input", &l->second, ImGuiInputTextFlags_CallbackEdit,
                     &updatePlaceName, (void*)&l->first);
    ImGui::PopItemWidth();
    ImGui::Text("Marking");
    int token_id = 0;
    for (const auto& at : vm.tokens | std::views::filter([=](const auto& at) {
                            return at.place == idx;
                          })) {
      drawTokenLine(at, token_id++);
    }
    ImGui::PopID();
  }
  static std::map<size_t, std::string>
      priorities;  // in-place buffer for changes.
  if (not vm.t_highlight.empty()) ImGui::Separator();
  for (auto idx : vm.t_highlight) {
    ImGui::PushID(idx);
    ImGui::Text("Transition");
    ImGui::SameLine();
    ImGui::PushItemWidth(-5);
    const auto [l, inserted] =
        names.try_emplace(idx + 1000, vm.net.transition[idx]);
    ImGui::InputText("##input", &l->second, ImGuiInputTextFlags_CallbackEdit,
                     &updateTransitionName, (void*)&l->first);
    ImGui::PopItemWidth();
    ImGui::Text("Priority");
    ImGui::SameLine();
    const auto [l_priority, inserted_priority] =
        priorities.try_emplace(idx, std::to_string(vm.net.priority[idx]));
    ImGui::InputText("##priority", &l_priority->second,
                     ImGuiInputTextFlags_CharsDecimal |
                         ImGuiInputTextFlags_CharsNoBlank |
                         ImGuiInputTextFlags_CallbackEdit,
                     &updatePriority, (void*)&l_priority->first);
    ImGui::Text("Output");
    ImGui::SameLine();
    if (ImGui::BeginCombo("##output", vm.net.output[idx].toString().data())) {
      for (const auto& color : vm.colors) {
        if (ImGui::Selectable(color.data())) {
          updateTransitionOutputColor(idx, symmetri::Token(color.data()));
        }
      }
      ImGui::EndCombo();
    }
    ImGui::PopID();
  }
  if (not vm.arc_highlight.empty()) ImGui::Separator();

  for (const auto& [node_type, selected_idx, sub_idx] : vm.arc_highlight) {
    const auto color = (node_type == model::Model::NodeType::Place
                            ? vm.net.input_n
                            : vm.net.output_n)[selected_idx][sub_idx]
                           .color;

    if (node_type == model::Model::NodeType::Place) {
      drawColorDropdownMenu(std::string(color.toString()), vm.colors,
                            [=](auto c) {
                              updateArcColor(node_type, selected_idx, sub_idx,
                                             symmetri::Token(c.data()));
                            });
    } else {
      ImGui::Text("%s", color.toString().data());
    }
  }
}

void draw_tools_menu(const model::ViewModel& vm) {
  ImGui::SetNextWindowSize(
      ImVec2(vm.kMenuWidth, ImGui::GetIO().DisplaySize.y - vm.kMenuBarHeight));
  ImGui::SetNextWindowPos(ImVec2(0, vm.kMenuBarHeight));

  ImGui::Begin("tools", NULL, no_move_draw_resize);
  // is now also true if there's nothing selected.
  const bool is_a_node_selected = vm.selected_node_idx.has_value();
  const auto invalid_index =
      std::max(vm.net.transition.size(), vm.net.place.size());
  const model::Model::NodeType node_type =
      std::get<0>(vm.selected_node_idx.value_or(
          std::make_tuple(model::Model::NodeType::Place, invalid_index)));

  const size_t selected_idx = is_a_node_selected
                                  ? std::get<1>(vm.selected_node_idx.value())
                                  : invalid_index;
  draw_simulation_menu(vm);
  draw_selection_menu(vm);
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
      //  if (is a node)
      for (size_t idx = 0; idx < vm.net.place.size(); ++idx) {
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
      for (size_t idx = 0; idx < vm.net.transition.size(); ++idx) {
        renderNodeEntry(
            model::Model::NodeType::Transition, vm.net.transition[idx], idx,
            is_a_node_selected && model::Model::NodeType::Place != node_type &&
                idx == selected_idx);
      }

      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Marking")) {
      for (const auto& [place, color, data] : vm.tokens) {
        ImGui::Text("%s,", vm.net.place[place].c_str());
        ImGui::SameLine();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(getColor(color)),
                           "%s", color.toString().data());
      }
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Eventlog")) {
      for (auto it = vm.log.rbegin(); it != vm.log.rend(); ++it) {
        ImGui::TextUnformatted(vm.net.transition[it->transition].c_str());
        ImGui::SameLine();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(getColor(it->state)),
                           "%s", it->state.toString().data());
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
  ImGui::End();
}
