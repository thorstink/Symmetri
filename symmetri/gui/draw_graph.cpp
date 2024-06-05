#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "draw_graph.h"

#include <math.h>  // fmodf

#include <algorithm>
#include <optional>
#include <ranges>
#include <string_view>

#include "color.hpp"
#include "graph_reducers.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "rxdispatch.h"
#include "symmetri/colors.hpp"

// State
static ImVec2 size;
static ImVec2 offset;

static const ImVec2 NODE_WINDOW_PADDING(8.0f, 8.0f);

static ImVec2 GetCenterPos(const ImVec2& pos, const ImVec2& size) {
  return ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
}

ImU32 getColor(symmetri::Token token) {
  using namespace symmetri;
  static std::unordered_map<Token, ImU32> color_table;
  const auto ptr = color_table.find(token);
  if (ptr != std::end(color_table)) {
    return ptr->second;
  } else {
    const auto rgb = hsv_to_rgb(ratio(), 0.6, 0.95);
    const auto color = IM_COL32(rgb[0], rgb[1], rgb[2], 128);
    color_table.insert({token, color});
    return color;
  }
};

void draw_grid(const ImVec2& scrolling) {
  auto draw_list = ImGui::GetWindowDrawList();
  ImU32 GRID_COLOR = IM_COL32(200, 200, 200, 40);
  float GRID_SZ = 64.0f;
  ImVec2 win_pos = ImGui::GetCursorScreenPos();
  ImVec2 canvas_sz = ImGui::GetWindowSize();
  for (float x = fmodf(scrolling.x, GRID_SZ); x < canvas_sz.x; x += GRID_SZ)
    draw_list->AddLine(ImVec2(x, 0.0f) + win_pos,
                       ImVec2(x, canvas_sz.y) + win_pos, GRID_COLOR);
  for (float y = fmodf(scrolling.y, GRID_SZ); y < canvas_sz.y; y += GRID_SZ)
    draw_list->AddLine(ImVec2(0.0f, y) + win_pos,
                       ImVec2(canvas_sz.x, y) + win_pos, GRID_COLOR);
};

void draw_arc(size_t t_idx, const model::ViewModel& vm) {
  const auto draw = [&](const symmetri::AugmentedToken& t, bool is_input,
                        size_t sub_idx) {
    if (std::find(vm.p_view.begin(), vm.p_view.end(), t.place) ==
        vm.p_view.end()) {
      return;
    }

    const ImVec2 i = offset + GetCenterPos(!is_input ? vm.t_positions[t_idx]
                                                     : vm.p_positions[t.place],
                                           size);
    const ImVec2 o = offset + GetCenterPos(is_input ? vm.t_positions[t_idx]
                                                    : vm.p_positions[t.place],
                                           size);

    const float max_distance = 2.f;
    const auto mouse_pos = ImGui::GetIO().MousePos;
    ImVec2 mouse_pos_projected_on_segment = ImLineClosestPoint(i, o, mouse_pos);
    ImVec2 mouse_pos_delta_to_segment =
        mouse_pos_projected_on_segment - mouse_pos;
    const bool is_segment_hovered = (ImLengthSqr(mouse_pos_delta_to_segment) <=
                                     max_distance * max_distance);
    if (is_segment_hovered && ImGui::IsMouseClicked(0)) {
      setSelectedArc(is_input, t_idx, sub_idx);
    }

    const auto is_selected_arc = [&]() {
      return vm.selected_arc_idxs.has_value() &&
             std::get<0>(*vm.selected_arc_idxs) == is_input &&
             std::get<1>(*vm.selected_arc_idxs) == t_idx &&
             std::get<2>(*vm.selected_arc_idxs) == sub_idx;
    };
    ImU32 imcolor =
        getColor(t.color) |
        ((ImU32)IM_F32_TO_INT8_SAT(is_selected_arc() ? 1.0f : 0.65f))
            << IM_COL32_A_SHIFT;

    auto draw_list = ImGui::GetWindowDrawList();

    const auto d = o - i;
    const auto theta = std::atan2(d.y, d.x) - M_PI_2;
    const float h = draw_list->_Data->FontSize * 1.00f;
    const float r = h * 0.40f * 1;
    const ImVec2 center = i + d * 0.6f;
    const auto a_sin = std::sin(theta);
    const auto a_cos = std::cos(theta);
    const auto a = ImRotate(ImVec2(+0.000f, +1.f) * r, a_cos, a_sin);
    const auto b = ImRotate(ImVec2(-1.f, -1.f) * r, a_cos, a_sin);
    const auto c = ImRotate(ImVec2(+1.f, -1.f) * r, a_cos, a_sin);
    draw_list->AddTriangleFilled(center + a, center + b, center + c, imcolor);
    draw_list->AddLine(i, o, imcolor, is_segment_hovered ? 3.0f : 2.0f);
  };

  for (size_t sub_idx = 0; sub_idx < vm.net.input_n[t_idx].size(); sub_idx++) {
    draw(vm.net.input_n[t_idx][sub_idx], true, sub_idx);
  }
  for (size_t sub_idx = 0; sub_idx < vm.net.output_n[t_idx].size(); sub_idx++) {
    draw(vm.net.output_n[t_idx][sub_idx], false, sub_idx);
  }
};

void draw_nodes(bool is_place, size_t idx, const std::string& name,
                const ImVec2& position, bool highlight) {
  ImGui::PushID(is_place ? idx + 10000 : idx);
  ImVec2 node_rect_min = offset + position;

  // Display node contents first
  auto textWidth = ImGui::CalcTextSize(name.c_str()).x;
  ImGui::SetCursorScreenPos(node_rect_min + NODE_WINDOW_PADDING +
                            ImVec2(8.0f - textWidth * 0.5f, -20.0f));
  ImGui::BeginGroup();  // Lock horizontal position
  ImGui::Text("%s", name.c_str());
  ImGui::EndGroup();

  // Save the size of what we have emitted and whether any of the widgets are
  // being used
  size = ImGui::GetItemRectSize() + NODE_WINDOW_PADDING + NODE_WINDOW_PADDING;
  size.x = size.y;
  ImVec2 node_rect_max = node_rect_min + size;

  // Display node box
  ImGui::SetCursorScreenPos(node_rect_min);
  ImGui::InvisibleButton("node", size);

  const bool node_moving_active = ImGui::IsItemActive();
  const bool is_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
  if (node_moving_active && is_clicked) {
    setSelectedNode(is_place, idx);
  } else if (node_moving_active &&
             ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    moveNode(is_place, idx, ImGui::GetIO().MouseDelta);
  }

  const int opacity = 255;
  auto draw_list = ImGui::GetWindowDrawList();
  auto select_color = highlight ? IM_COL32(255, 255, 0, opacity)
                                : IM_COL32(100, 100, 100, opacity);

  if (is_place) {
    draw_list->AddCircleFilled(offset + GetCenterPos(position, size),
                               0.5f * size.x, IM_COL32(135, 135, 135, opacity),
                               -5);
    draw_list->AddCircle(offset + GetCenterPos(position, size), 0.5f * size.x,
                         select_color, -5, 3.0f);

  } else {
    draw_list->AddRectFilled(node_rect_min, node_rect_max,
                             IM_COL32(200, 200, 200, opacity), 4.0f);
    draw_list->AddRect(node_rect_min, node_rect_max, select_color, 4.0f, 0,
                       3.0f);
  }

  ImGui::PopID();
};

void draw_everything(const model::ViewModel& vm) {
  // is now also true if there's nothing selected.
  const bool is_a_node_selected = vm.selected_node_idx.has_value();
  const bool is_place = vm.selected_node_idx.has_value() &&
                        std::get<0>(vm.selected_node_idx.value());
  const size_t selected_idx =
      is_a_node_selected ? std::get<1>(vm.selected_node_idx.value()) : 9999;

  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
      !ImGui::IsAnyItemHovered()) {
    setContextMenuInactive();
    if (vm.selected_node_idx.has_value() || vm.selected_arc_idxs.has_value()) {
      resetSelection();
    }
  }

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
  ImGui::SameLine();
  ImGui::BeginGroup();

  // Create our child canvas
  ImGui::Text("%s", vm.active_file.c_str());

  ImGui::SameLine();
  const auto io = ImGui::GetIO();
  ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
              1000.0f / io.Framerate, io.Framerate);
  ImGui::SameLine(ImGui::GetWindowWidth() - 340);
  static bool yes;
  ImGui::Checkbox("Show grid", &yes);
  // ImGui::Checkbox("Show grid", &vm.show_grid);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(60, 60, 70, 200));
  ImGui::BeginChild("scrolling_region", ImVec2(0, 0), true,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);
  ImGui::PopStyleVar();  // WindowPadding
  ImGui::PushItemWidth(120.0f);

  offset = ImGui::GetCursorScreenPos() + vm.scrolling;
  auto draw_list = ImGui::GetWindowDrawList();

  // Display grid
  if (vm.show_grid) {
    draw_grid(vm.scrolling);
  }

  // draw places & transitions
  for (auto&& idx : vm.t_view) {
    draw_arc(idx, vm);
    draw_nodes(false, idx, vm.net.transition[idx], vm.t_positions[idx],
               is_a_node_selected && !is_place && idx == selected_idx);
  }
  for (auto&& idx : vm.p_view) {
    draw_nodes(true, idx, vm.net.place[idx], vm.p_positions[idx],
               is_a_node_selected && is_place && idx == selected_idx);
  }

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
      ImVec2 scene_pos = ImGui::GetMousePosOnOpeningCurrentPopup() - offset;
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
