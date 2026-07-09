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
#include "path_autocomplete.h"
#include "petri.h"
#include "reducers.h"
#include "shared.h"
#include "symmetri/callback.h"
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

static ImU32 applyOpacity(ImU32 color, float opacity) {
  unsigned int alpha = static_cast<unsigned int>(opacity * 255);
  unsigned int r = (color >> 0) & 0xFF;
  unsigned int g = (color >> 8) & 0xFF;
  unsigned int b = (color >> 16) & 0xFF;
  return (alpha << 24) | (b << 16) | (g << 8) | (r << 0);
}

bool draw_bezier_arc(const ImVec2& src, const ImVec2& dst, float perp_offset,
                     ImU32 color, float opacity, float target_radius,
                     bool filled_arrow = true) {
  const ImVec2 d = dst - src;
  const float len = ImSqrt(ImLengthSqr(d));
  if (len < 1.0f) return false;

  const ImVec2 dir = d / len;
  const ImVec2 perp = ImVec2(-dir.y, dir.x);

  const float ctrl_dist = len * 0.4f;
  const ImVec2 cp1 = src + dir * ctrl_dist + perp * perp_offset;
  const ImVec2 cp2 = dst - dir * ctrl_dist + perp * perp_offset;

  const auto mouse_pos = ImGui::GetIO().MousePos;
  const float tess_tol = ImGui::GetStyle().CurveTessellationTol;
  const ImVec2 closest = ImBezierCubicClosestPointCasteljau(
      src, cp1, cp2, dst, mouse_pos, tess_tol);
  const float max_distance = 4.0f;
  const bool is_hovered =
      not ImGui::IsAnyItemHovered() &&
      ImLengthSqr(closest - mouse_pos) <= max_distance * max_distance;

  color = applyOpacity(color, opacity);

  auto draw_list = ImGui::GetWindowDrawList();
  draw_list->AddBezierCubic(src, cp1, cp2, dst, color,
                            is_hovered ? 5.0f : 2.5f);

  const ImVec2 tangent = dst - cp2;
  const float tang_len = ImSqrt(ImLengthSqr(tangent));
  if (tang_len > 0.1f) {
    const float h = draw_list->_Data->FontSize * 1.00f;
    const float r = h * 0.40f;
    const ImVec2 tang_dir = tangent / tang_len;
    const ImVec2 tang_perp = ImVec2(-tang_dir.y, tang_dir.x);
    const ImVec2 arrow_tip = dst - tang_dir * target_radius;
    const ImVec2 arrow_base = arrow_tip - tang_dir * (2.0f * r);
    if (filled_arrow)
      draw_list->AddTriangleFilled(arrow_tip, arrow_base + tang_perp * r,
                                   arrow_base - tang_perp * r, color);
    else
      draw_list->AddTriangle(arrow_tip, arrow_base + tang_perp * r,
                             arrow_base - tang_perp * r, color, 2.0f);
  }

  return is_hovered;
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

  const ImVec2 node_pad(vm.node_size.x, vm.node_size.y);
  const model::Coordinate scaled_scroll = vm.zoom_factor * vm.scrolling;
  const model::Coordinate scaled_t_pos = vm.zoom_factor * vm.t_positions[t_idx];
  const ImVec2 t_center = scaled_scroll + GetCenterPos(scaled_t_pos, node_pad);

  // Compute perpendicular offset for an arc without any heap allocation.
  // Scans input_n then output_n to find this arc's rank among all arcs
  // sharing the same (t_idx, p_idx) pair.
  const auto perp_offset_for = [&](size_t p_idx, bool is_input,
                                   size_t sub_idx) {
    const float spread = 30.0f * vm.zoom_factor;
    size_t n = 0, k = 0;
    for (size_t i = 0; i < vm.net.input_n[t_idx].size(); i++) {
      if (vm.net.input_n[t_idx][i].place == p_idx) {
        if (is_input && i == sub_idx) k = n;
        n++;
      }
    }
    for (size_t i = 0; i < vm.net.output_n[t_idx].size(); i++) {
      if (vm.net.output_n[t_idx][i].place == p_idx) {
        if (!is_input && i == sub_idx) k = n;
        n++;
      }
    }
    return (static_cast<float>(k) - (n - 1) * 0.5f) * spread;
  };

  for (size_t sub_idx = 0; sub_idx < vm.net.input_n[t_idx].size(); sub_idx++) {
    const auto& [p_idx, color, data] = vm.net.input_n[t_idx][sub_idx];
    const float opacity =
        should_highlight(model::Model::NodeType::Place, t_idx, sub_idx) ? 1.0f
                                                                        : 0.5f;
    const model::Coordinate scaled_p_pos =
        vm.zoom_factor * vm.p_positions[p_idx];
    const ImVec2 p_center =
        scaled_scroll + GetCenterPos(scaled_p_pos, node_pad);
    const float offset = perp_offset_for(p_idx, true, sub_idx);

    if (draw_bezier_arc(p_center, t_center, offset, getColor(color), opacity,
                        node_pad.x * 0.5f)) {
      an_arc_is_hovered = true;
      if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        (ImGui::IsKeyDown(ImGuiKey_LeftShift)
             ? addHighlightArc
             : setSelectedArc)(model::Model::NodeType::Place, t_idx, sub_idx);
      }
    }
  }

  const ImU32 output_color = getColor(vm.net.output[t_idx]);
  for (size_t sub_idx = 0; sub_idx < vm.net.output_n[t_idx].size(); sub_idx++) {
    const size_t p_idx = vm.net.output_n[t_idx][sub_idx].place;
    const float opacity =
        should_highlight(model::Model::NodeType::Transition, t_idx, sub_idx)
            ? 1.0f
            : 0.5f;
    const model::Coordinate scaled_p_pos =
        vm.zoom_factor * vm.p_positions[p_idx];
    const ImVec2 p_center =
        scaled_scroll + GetCenterPos(scaled_p_pos, node_pad);
    const float offset = perp_offset_for(p_idx, false, sub_idx);

    // Negate offset: output arcs travel in the opposite direction to input
    // arcs, which flips the perpendicular vector, so we compensate to keep arcs
    // on consistent sides relative to the canonical p→t axis.
    if (draw_bezier_arc(t_center, p_center, -offset, output_color, opacity,
                        node_pad.x * 0.5f, false)) {
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
                const std::vector<symmetri::AugmentedToken>& tokens,
                float zoom_factor) {
  ImGui::PushID(model::Model::NodeType::Place == node_type ? idx + 10000 : idx);
  ImVec2 node_rect_min = offset + ImVec2(position.x, position.y);

  ImGui::PushFont(NULL, zoom_factor * ImGui::GetFontSize());
  const ImVec2 text_size = ImGui::CalcTextSize(name.c_str());
  const float gap = node_window_padding.y * 0.5f;
  ImGui::SetCursorScreenPos(
      ImVec2(node_rect_min.x + 0.5f * (node_window_padding.x - text_size.x),
             node_rect_min.y - gap - text_size.y));
  ImGui::Text("%s", name.c_str());
  ImGui::PopFont();

  // Display node box
  ImGui::SetCursorScreenPos(node_rect_min);
  ImGui::InvisibleButton("node", node_window_padding);

  if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    (ImGui::IsKeyDown(ImGuiKey_LeftShift) ? addHighlightNode : setSelectedNode)(
        node_type, idx);
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
    const auto is_token_in_place = [idx](const auto& token) {
      return token.place == idx;
    };
    const size_t tokens_in_place =
        std::ranges::count_if(tokens, is_token_in_place);
    const auto coordinates = getTokenCoordinates(tokens_in_place);
    if (not coordinates.empty()) {
      int i = 0;
      for (auto color :
           tokens | std::views::filter(is_token_in_place) |
               std::views::transform([](const auto& t) { return t.color; })) {
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
    const ImVec2 world_pos = ImGui::GetIO().MousePos / vm.zoom_factor -
                             ImVec2(vm.scrolling.x, vm.scrolling.y);
    setContextMenuActive(world_pos);
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

  farbart::drawPathInput(vm);

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
  for (size_t idx = 0; idx < vm.net.transition.size(); ++idx) {
    if (auto arc_is_hovered = draw_arcs(idx, vm)) {
      an_arc_is_hovered = arc_is_hovered;
    }

    const bool should_hightlight =
        std::find(vm.t_highlight.cbegin(), vm.t_highlight.cend(), idx) !=
        vm.t_highlight.cend();

    draw_nodes(model::Model::NodeType::Transition, idx, vm.net.transition[idx],
               vm.zoom_factor * vm.t_positions[idx],
               vm.zoom_factor * vm.scrolling,
               ImVec2(vm.node_size.x, vm.node_size.y), should_hightlight,
               vm.tokens, vm.zoom_factor);
  }
  if (vm.arc_hovered != an_arc_is_hovered) {
    setArcHoverState(an_arc_is_hovered);
  }

  for (size_t idx = 0; idx < vm.net.place.size(); ++idx) {
    const bool should_hightlight =
        std::find(vm.p_highlight.cbegin(), vm.p_highlight.cend(), idx) !=
        vm.p_highlight.cend();

    draw_nodes(model::Model::NodeType::Place, idx, vm.net.place[idx],
               vm.zoom_factor * vm.p_positions[idx],
               vm.zoom_factor * vm.scrolling,
               ImVec2(vm.node_size.x, vm.node_size.y), should_hightlight,
               vm.tokens, vm.zoom_factor);
  }

  if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    // Pass a world-space delta: the edit reducer that moves the node must not
    // read view state (zoom) so it stays replayable for undo.
    moveNode(vm.t_highlight, vm.p_highlight,
             ImGui::GetIO().MouseDelta / vm.zoom_factor);
  }
  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
  ImGui::End();
}
