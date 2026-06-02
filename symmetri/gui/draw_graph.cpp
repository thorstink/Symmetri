#define IMGUI_DEFINE_MATH_OPERATORS

#include "draw_graph.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <ranges>
#include <string>
#include <vector>

#include "draw_context_menu.h"
#include "draw_marking.hpp"
#include "gui/model.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "petri.h"
#include "reducers.h"
#include "shared.h"
#include "symmetri/colors.hpp"

static ImVec2 GetCenterPos(const model::Coordinate& pos, const ImVec2& size) {
  return ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
}

ImVec2 operator+(const model::Coordinate& pos, const ImVec2& y) {
  return ImVec2(pos.x, pos.y) + y;
}
ImVec2 operator+(const ImVec2& y, const model::Coordinate& pos) {
  return ImVec2(pos.x, pos.y) + y;
}

ImVec2 operator-(const model::Coordinate& pos, const ImVec2& y) {
  return ImVec2(pos.x, pos.y) - y;
}
ImVec2 operator-(const ImVec2& y, const model::Coordinate& pos) {
  return ImVec2(pos.x, pos.y) - y;
}

bool draw_line(const model::Coordinate& source_pos,
               const model::Coordinate& target_pos,
               const model::Coordinate& offset,
               const ImVec2& node_window_padding, ImU32 color, float opacity) {
  const ImVec2 o = offset + GetCenterPos(source_pos, node_window_padding);
  const ImVec2 i = offset + GetCenterPos(target_pos, node_window_padding);

  const float max_distance = 2.f;
  const auto mouse_pos = ImGui::GetIO().MousePos;
  ImVec2 mouse_pos_projected_on_segment = ImLineClosestPoint(i, o, mouse_pos);
  ImVec2 mouse_pos_delta_to_segment =
      mouse_pos_projected_on_segment - mouse_pos;
  const bool is_segment_hovered =
      not ImGui::IsAnyItemHovered() &&
      (ImLengthSqr(mouse_pos_delta_to_segment) <= max_distance * max_distance);

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
  {
    // Extract existing color components (assuming A-B-G-R format)
    unsigned int alpha = static_cast<unsigned int>(opacity * 255);
    unsigned int r = (color >> 0) & 0xFF;
    unsigned int g = (color >> 8) & 0xFF;
    unsigned int b = (color >> 16) & 0xFF;
    color = (alpha << 24) | (b << 16) | (g << 8) | (r << 0);
  }

  // Combine with the new alpha value
  draw_list->AddTriangleFilled(center + a, center + b, center + c, color);
  draw_list->AddLine(i, o, color, is_segment_hovered ? 5.0f : 2.5f);
  return is_segment_hovered;
};

void draw_grid(const model::ViewModel& vm) {
  auto draw_list = ImGui::GetWindowDrawList();
  ImU32 GRID_COLOR = IM_COL32(200, 200, 200, 80);
  float GRID_SZ = 64.0f;
  ImVec2 win_pos = ImGui::GetCursorScreenPos();
  ImVec2 canvas_sz = ImGui::GetWindowSize();
  for (float x = fmodf(vm.scrolling.x, GRID_SZ); x < canvas_sz.x; x += GRID_SZ)
    draw_list->AddLine(ImVec2(x, 0.0f) + win_pos,
                       ImVec2(x, canvas_sz.y) + win_pos, GRID_COLOR);
  for (float y = fmodf(vm.scrolling.y, GRID_SZ); y < canvas_sz.y; y += GRID_SZ)
    draw_list->AddLine(ImVec2(0.0f, y) + win_pos,
                       ImVec2(canvas_sz.x, y) + win_pos, GRID_COLOR);
};

bool draw_arcs(size_t t_idx, const model::ViewModel& vm) {
  const auto should_highlight = [&](auto source_node_type, size_t t_idx,
                                    size_t sub_idx) {
    return std::find(vm.arc_highlight.cbegin(), vm.arc_highlight.cend(),
                     model::Model::Arc{source_node_type, t_idx, sub_idx}) !=
           vm.arc_highlight.cend();
  };
  bool an_arc_is_hovered = false;

  for (size_t sub_idx = 0; sub_idx < vm.net.input_n[t_idx].size(); sub_idx++) {
    const auto& [p_idx, color] = vm.net.input_n[t_idx][sub_idx];
    float opacity =
        should_highlight(model::Model::NodeType::Place, t_idx, sub_idx) ? 1.0f
                                                                        : 0.5f;
    if (draw_line(vm.zoom_factor * vm.t_positions[t_idx],
                  vm.zoom_factor * vm.p_positions[p_idx],
                  vm.zoom_factor * vm.scrolling,
                  ImVec2(vm.node_size.x, vm.node_size.y), getColor(color),
                  opacity)) {
      an_arc_is_hovered = true;
      if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        (ImGui::IsKeyDown(ImGuiKey_LeftShift)
             ? addHighlightArc
             : setSelectedArc)(model::Model::NodeType::Place, t_idx, sub_idx);
      }
    }
  }

  for (size_t sub_idx = 0; sub_idx < vm.net.output_n[t_idx].size(); sub_idx++) {
    const auto& [p_idx, color] = vm.net.output_n[t_idx][sub_idx];
    float opacity =
        should_highlight(model::Model::NodeType::Transition, t_idx, sub_idx)
            ? 1.0f
            : 0.5f;

    if (draw_line(vm.zoom_factor * vm.p_positions[p_idx],
                  vm.zoom_factor * vm.t_positions[t_idx],
                  vm.zoom_factor * vm.scrolling,
                  ImVec2(vm.node_size.x, vm.node_size.y), getColor(color),
                  opacity)) {
      an_arc_is_hovered = true;
      if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        (ImGui::IsKeyDown(ImGuiKey_LeftShift) ? addHighlightArc
                                              : setSelectedArc)(
            model::Model::NodeType::Transition, t_idx, sub_idx);
      }
    }
  }

  return an_arc_is_hovered;
};

void draw_nodes(model::Model::NodeType node_type, size_t idx,
                const std::string& name, const model::Coordinate& position,
                const model::Coordinate& offset,
                const ImVec2& node_window_padding, bool highlight,
                const std::vector<symmetri::AugmentedToken>& tokens) {
  ImGui::PushID(model::Model::NodeType::Place == node_type ? idx + 10000 : idx);
  ImVec2 node_rect_min = offset + ImVec2(position.x, position.y);

  ImGui::PushFont(NULL, 0.75f * ImGui::GetFontSize());
  ImVec2 textWidth = ImGui::CalcTextSize(name.c_str());
  ImGui::SetCursorScreenPos(node_rect_min -
                            ImVec2(0.5f * (textWidth.x - node_window_padding.x),
                                   node_window_padding.y));
  ImGui::Text("%s", name.c_str());
  ImGui::PopFont();

  // Display node box
  ImGui::SetCursorScreenPos(node_rect_min);
  ImGui::InvisibleButton("node", node_window_padding);

  if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    (ImGui::IsKeyDown(ImGuiKey_LeftShift) ? addHighlightNode : setSelectedNode)(
        node_type, idx);
  } else if (ImGui::IsItemActive() &&
             ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    moveNode(node_type, idx, ImGui::GetIO().MouseDelta);
  }

  const int opacity = 255;
  auto draw_list = ImGui::GetWindowDrawList();
  auto select_color = highlight ? IM_COL32(255, 255, 0, opacity)
                                : IM_COL32(100, 100, 100, opacity);

  if (node_type == model::Model::NodeType::Place) {
    draw_list->AddCircleFilled(
        offset + GetCenterPos(position, node_window_padding),
        0.5f * node_window_padding.x, IM_COL32(200, 200, 200, opacity), -5);
    draw_list->AddCircle(offset + GetCenterPos(position, node_window_padding),
                         0.5f * node_window_padding.x, select_color, -5, 3.0f);
    const auto is_token_in_place = [idx](const auto token) {
      return std::get<size_t>(token) == idx;
    };
    const size_t tokens_in_place =
        std::ranges::count_if(tokens, is_token_in_place);
    const auto coordinates = getTokenCoordinates(tokens_in_place);
    if (not coordinates.empty()) {
      int i = 0;
      for (auto color : tokens | std::views::filter(is_token_in_place) |
                            std::views::transform([](const auto& t) {
                              return std::get<symmetri::Token>(t);
                            })) {
        draw_list->AddCircle(coordinates[i] + offset +
                                 GetCenterPos(position, node_window_padding),
                             0.08f * node_window_padding.x,
                             IM_COL32(250, 250, 250, 255), -5, 3.0f);
        draw_list->AddCircleFilled(
            coordinates[i++] + offset +
                GetCenterPos(position, node_window_padding),
            0.07f * node_window_padding.x, getColor(color), -5.0f);
      }
    } else if (tokens_in_place > 0) {
      const auto t = std::format("{}", tokens_in_place);
      ImVec2 textWidth = ImGui::CalcTextSize(t.c_str());
      ImGui::SetCursorScreenPos(
          offset + GetCenterPos(position, node_window_padding) -
          ImVec2(0.5f * (textWidth.x), 0.5f * (textWidth.y)));
      ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(select_color), "%s",
                         t.c_str());
    }
  } else {
    ImVec2 node_rect_max = node_rect_min + node_window_padding;

    draw_list->AddRectFilled(node_rect_min, node_rect_max,
                             IM_COL32(135, 135, 135, opacity), 4.0f);
    draw_list->AddRect(node_rect_min, node_rect_max, select_color, 4.0f, 0,
                       3.0f);
  }

  ImGui::PopID();
};

void draw_graph(const model::ViewModel& vm) {
  ImGui::SetNextWindowSize(
      ImVec2(ImGui::GetIO().DisplaySize.x - vm.kMenuWidth,
             ImGui::GetIO().DisplaySize.y - vm.kMenuBarHeight));
  ImGui::SetNextWindowPos(ImVec2(vm.kMenuWidth, vm.kMenuBarHeight));
  ImGui::Begin("graph", NULL,
               no_move_draw_resize | ImGuiWindowFlags_NoBringToFrontOnFocus);

  if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) &&
      ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
    setContextMenuActive();
  } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
             not ImGui::IsAnyItemHovered()) {
    if (std::find(vm.drawables.begin(), vm.drawables.end(),
                  &draw_context_menu) != vm.drawables.end()) {
      setContextMenuInactive();
    }
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
        not vm.arc_hovered) {
      resetSelection();
    }
  } else if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
             ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0f)) {
    // Scrolling
    moveView(ImGui::GetIO().MouseDelta);
  }

  static char view_name[256] = "";
  strcpy(view_name, vm.active_file.c_str());
  ImGui::PushItemWidth(-1);

  // currently we render the text green if the file does not yet exist.
  const bool file_exists =
      std::filesystem::is_regular_file(std::filesystem::path(vm.active_file));
  if (not file_exists) {
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
  }
  ImGui::InputText("##filepath", view_name, 256,
                   ImGuiInputTextFlags_CallbackEdit, &updateActiveFile);
  if (not file_exists) {
    ImGui::PopStyleColor();
  }

  ImGui::PopItemWidth();

  const auto io = ImGui::GetIO();
  ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
              1000.0f / io.Framerate, io.Framerate);

  ImGui::SameLine();
  bool show_grid = vm.show_grid;
  if (ImGui::Checkbox("Show grid", &show_grid)) {
    showGrid(show_grid);
  }

  static float zoom_slider_value = vm.zoom_factor;
  ImGui::SameLine();
  ImVec2 textWidth = ImGui::CalcTextSize("Zoom");
  ImGui::PushItemWidth(-textWidth.x);
  ImGui::SliderFloat("Zoom", &zoom_slider_value, 0.5f, 1.5f);
  if (ImGui::IsItemActive() && zoom_slider_value != vm.zoom_factor) {
    zoomAbsolute(zoom_slider_value);
  }

  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(60, 60, 70, 100));
  ImGui::BeginChild("scrolling_region", ImVec2(0, 0), true,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);
  if (vm.show_grid) {
    draw_grid(vm);
  }
  // draw places & transitions
  bool an_arc_is_hovered = false;
  for (auto&& idx : vm.t_view) {
    if (auto arc_is_hovered = draw_arcs(idx, vm)) {
      an_arc_is_hovered = arc_is_hovered;
    }

    const bool should_hightlight =
        std::find(vm.t_highlight.cbegin(), vm.t_highlight.cend(), idx) !=
        vm.t_highlight.cend();

    draw_nodes(
        model::Model::NodeType::Transition, idx, vm.net.transition[idx],
        vm.zoom_factor * vm.t_positions[idx], vm.zoom_factor * vm.scrolling,
        ImVec2(vm.node_size.x, vm.node_size.y), should_hightlight, vm.tokens);
  }
  if (vm.arc_hovered != an_arc_is_hovered) {
    setArcHoverState(an_arc_is_hovered);
  }

  for (auto idx : vm.p_view) {
    const bool should_hightlight =
        std::find(vm.p_highlight.cbegin(), vm.p_highlight.cend(), idx) !=
        vm.p_highlight.cend();

    draw_nodes(
        model::Model::NodeType::Place, idx, vm.net.place[idx],
        vm.zoom_factor * vm.p_positions[idx], vm.zoom_factor * vm.scrolling,
        ImVec2(vm.node_size.x, vm.node_size.y), should_hightlight, vm.tokens);
  }

  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
  ImGui::End();
}
