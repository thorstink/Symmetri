#include <iostream>
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include <cmath>

#include "drawable.h"
#include "graph.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "redux.hpp"

// Creating a node graph editor for Dear ImGui
// Quick sample, not production code!
// This is quick demo I crafted in a few hours in 2015 showcasing how to use
// Dear ImGui to create custom stuff, which ended up feeding a thread full of
// better experiments. See https://github.com/ocornut/imgui/issues/306 for
// details

// Fast forward to 2023, see e.g.
// https://github.com/ocornut/imgui/wiki/Useful-Extensions#node-editors

// Changelog
// - v0.05 (2023-03): fixed for renamed api: AddBezierCurve()->AddBezierCubic().
// - v0.04 (2020-03): minor tweaks
// - v0.03 (2018-03): fixed grid offset issue, inverted sign of 'scrolling'
// - v0.01 (2015-08): initial version

#include <math.h>  // fmodf

#include "symmetri/colors.hpp"

inline ImU32 getColor(symmetri::Token token) {
  using namespace symmetri;
  switch (token) {
    case Color::Scheduled:
    case Color::Started:
    case Color::Deadlocked:
    case Color::Canceled:
    case Color::Paused:
    case Color::Failed:
      return IM_COL32(255, 0, 0, 128);
      break;
    case Color::Success:
      return IM_COL32(0, 255, 0, 128);
      break;
    default: {
      // create new color and lookup if it already exists.
      return IM_COL32(255, 200, 0, 128);
    }
  }
};

// State
static ImVec2 scrolling = ImVec2(0.0f, 0.0f);
static bool show_grid = true;
static Symbol node_selected = Symbol(-1);
static ImVec2 size;
static ImVec2 offset;
static ImDrawList* draw_list;
static bool open_context_menu = false;
// static Symbol node_hovered_in_list = Symbol(-1);
// static Symbol node_hovered_in_scene = Symbol(-1);
static Node* node_hovered_in_list = nullptr;
static Node* node_hovered_in_scene = nullptr;
static Arc* active_arc = nullptr;
static const float NODE_SLOT_RADIUS = 4.0f;
static const ImVec2 NODE_WINDOW_PADDING(8.0f, 8.0f);

void draw_arc(Arc& arc) {
  const auto& [color, from_to] = arc;
  ImVec2 p1 = offset + Node::GetCenterPos(*from_to[0], size);
  ImVec2 p2 = offset + Node::GetCenterPos(*from_to[1], size);
  // use with:
  using namespace ImGui;
  ImU32 imcolor = getColor(color);
  const float max_distance = 2.f;
  const auto mouse_pos = ImGui::GetIO().MousePos;
  ImVec2 mouse_pos_projected_on_segment = ImLineClosestPoint(p1, p2, mouse_pos);
  ImVec2 mouse_pos_delta_to_segment =
      mouse_pos_projected_on_segment - mouse_pos;
  bool is_segment_hovered =
      active_arc == &arc ||
      (ImLengthSqr(mouse_pos_delta_to_segment) <= max_distance * max_distance);

  imcolor |= ((ImU32)IM_F32_TO_INT8_SAT(is_segment_hovered ? 1.0f : 0.65f))
             << IM_COL32_A_SHIFT;

  if (is_segment_hovered && ImGui::IsMouseClicked(0)) {
    active_arc = &arc;
  }

  const auto d = p2 - p1;
  const auto theta = std::atan2(d.y, d.x) - M_PI_2;
  const float h = draw_list->_Data->FontSize * 1.00f;
  const float r = h * 0.40f * 1;
  const ImVec2 center = p1 + d / 2.f;
  const auto a_sin = std::sin(theta);
  const auto a_cos = std::cos(theta);
  const auto a = ImRotate(ImVec2(+0.000f, +1.f) * r, a_cos, a_sin);
  const auto b = ImRotate(ImVec2(-1.f, -1.f) * r, a_cos, a_sin);
  const auto c = ImRotate(ImVec2(+1.f, -1.f) * r, a_cos, a_sin);
  draw_list->AddTriangleFilled(center + a, center + b, center + c, imcolor);
  draw_list->AddLine(p1, p2, imcolor, is_segment_hovered ? 3.0f : 2.0f);
};

void draw_nodes(Node& node) {
  open_context_menu = false;
  ImGui::PushID(node.id.key());
  ImVec2 node_rect_min = offset + node.Pos;

  // Display node contents first
  bool old_any_active = ImGui::IsAnyItemActive();
  auto textWidth = ImGui::CalcTextSize(node.name.c_str()).x;
  ImGui::SetCursorScreenPos(node_rect_min + NODE_WINDOW_PADDING +
                            ImVec2(8.0f - textWidth * 0.5f, -20.0f));
  ImGui::BeginGroup();  // Lock horizontal position
  ImGui::Text("%s", node.name.c_str());
  ImGui::EndGroup();

  // Save the size of what we have emitted and whether any of the widgets are
  // being used
  bool node_widgets_active = (!old_any_active && ImGui::IsAnyItemActive());
  size = ImGui::GetItemRectSize() + NODE_WINDOW_PADDING + NODE_WINDOW_PADDING;
  size.x = size.y;
  ImVec2 node_rect_max = node_rect_min + size;

  // Display node box
  ImGui::SetCursorScreenPos(node_rect_min);
  ImGui::InvisibleButton("node", size);
  node_hovered_in_scene = ImGui::IsItemHovered()      ? &node
                          : ImGui::IsAnyItemHovered() ? node_hovered_in_scene
                                                      : nullptr;
  bool node_moving_active = ImGui::IsItemActive();
  if (node_widgets_active || node_moving_active) {
    node_selected = node.id;
  }
  if (node_moving_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    node.Pos = node.Pos + ImGui::GetIO().MouseDelta;
  }
  const int opacity = 255;
  auto select_color =
      node.id == node_selected ? IM_COL32(255, 255, 0, opacity)
      : &node == node_hovered_in_list || &node == node_hovered_in_scene
          ? IM_COL32(255, 255, 50, 100)
          : IM_COL32(100, 100, 100, opacity);
  if (node.id.chr() == 'P') {
    draw_list->AddCircleFilled(offset + Node::GetCenterPos(node.Pos, size),
                               0.5f * size.x, IM_COL32(135, 135, 135, opacity),
                               -5);
    draw_list->AddCircle(offset + Node::GetCenterPos(node.Pos, size),
                         0.5f * size.x, select_color, -5, 3.0f);

  } else {
    draw_list->AddRectFilled(node_rect_min, node_rect_max,
                             IM_COL32(200, 200, 200, opacity), 4.0f);
    draw_list->AddRect(node_rect_min, node_rect_max, select_color, 4.0f, 0,
                       3.0f);
  }

  ImGui::PopID();
};

// Dummy data structure provided for the example.
// Note that we storing links as indices (not ID) to make example code shorter.
template <>
void draw(Graph& g) {
  ImVec2 WindowSize = ImGui::GetWindowSize();
  WindowSize.y -= 140.0f;
  // Draw a list of nodes on the left side
  ImGui::BeginChild("some", ImVec2(200, 0));
  ImGui::Text("Selected");
  ImGui::Separator();
  ImGui::BeginChild("selected_node", ImVec2(200, 0.1 * WindowSize.y));
  if (node_selected != Symbol(-1)) {
    auto node =
        std::find_if(g.nodes.begin(), g.nodes.end(),
                     [=](const auto& n) { return n.id == node_selected; });
    ImGui::Text("Name");
    ImGui::SameLine();
    const auto id = std::string("##") + std::to_string(node->id.key());
    ImGui::PushItemWidth(-1);
    ImGui::InputText(id.c_str(), node->name.data(), 30);
    ImGui::PopItemWidth();
    static int priority = 1;
    static int multiplicity = 1;
    if (node->id.chr() == 'T') {
      ImGui::Text("Priority");
      ImGui::SameLine();
      ImGui::InputInt("##", &priority);
      ImGui::Text("Weight");
      ImGui::SameLine();
      ImGui::InputInt("##", &multiplicity);
    }
  }
  ImGui::Separator();
  if (active_arc != nullptr) {
    constexpr std::size_t offset = offsetof(Node, Pos);
    const Node* from = reinterpret_cast<const Node*>(
        reinterpret_cast<const char*>(active_arc->from_to_pos[0]) - offset);
    const Node* to = reinterpret_cast<const Node*>(
        reinterpret_cast<const char*>(active_arc->from_to_pos[1]) - offset);

    ImGui::Text("From:");
    ImGui::SameLine();
    ImGui::PushItemWidth(-1);
    ImGui::Text("%s", from->name.data());
    ImGui::PopItemWidth();
    ImGui::Text("To:");
    ImGui::SameLine();
    ImGui::PushItemWidth(-1);
    ImGui::Text("%s", to->name.data());
    ImGui::PopItemWidth();
    ImGui::Text("Color");
    ImGui::SameLine();
    ImGui::Text("%s", symmetri::Color::toString(active_arc->color).c_str());
  }
  ImGui::EndChild();

  ImGui::Dummy(ImVec2(0.0f, 20.0f));

  ImGui::Text("Places");
  ImGui::Separator();
  constexpr float height_fraction = 0.8 / 2.0;
  ImGui::BeginChild("place_list", ImVec2(200, height_fraction * WindowSize.y));
  for (auto& node : g.nodes) {
    if (node.id.chr() == 'P') {
      ImGui::PushID(node.id.key());
      if (ImGui::Selectable(node.name.c_str(), node.id == node_selected)) {
        node_selected = node.id;
      }
      node_hovered_in_list = ImGui::IsItemHovered()      ? &node
                             : ImGui::IsAnyItemHovered() ? node_hovered_in_list
                                                         : nullptr;

      if (node_hovered_in_list) {
        // node_hovered_in_list = node.id;
        // node_hovered_in_list = node.id;
        open_context_menu |= ImGui::IsMouseClicked(1);
      }
      ImGui::PopID();
    }
  }
  ImGui::EndChild();

  ImGui::Dummy(ImVec2(0.0f, 20.0f));
  ImGui::Text("Transitions");
  ImGui::Separator();
  ImGui::BeginChild("transition_list",
                    ImVec2(200, height_fraction * WindowSize.y));
  for (auto& node : g.nodes) {
    if (node.id.chr() == 'T') {
      ImGui::PushID(node.id.key());
      if (ImGui::Selectable(node.name.c_str(), node.id == node_selected)) {
        node_selected = node.id;
      }
      node_hovered_in_list = ImGui::IsItemHovered()      ? &node
                             : ImGui::IsAnyItemHovered() ? node_hovered_in_list
                                                         : nullptr;
      // if (ImGui::IsItemHovered()) {
      // node_hovered_in_list = node.id;
      // open_context_menu |= ImGui::IsMouseClicked(1);
      // }
      ImGui::PopID();
    }
  }
  ImGui::EndChild();

  ImGui::EndChild();

  ImGui::SameLine();
  ImGui::BeginGroup();

  // Create our child canvas
  ImGui::Text("Hold middle mouse button to scroll (%.2f,%.2f)", scrolling.x,
              scrolling.y);
  ImGui::SameLine(ImGui::GetWindowWidth() - 100);
  ImGui::Checkbox("Show grid", &show_grid);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(60, 60, 70, 200));
  ImGui::BeginChild("scrolling_region", ImVec2(0, 0), true,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);
  ImGui::PopStyleVar();  // WindowPadding
  ImGui::PushItemWidth(120.0f);

  offset = ImGui::GetCursorScreenPos() + scrolling;
  draw_list = ImGui::GetWindowDrawList();

  // Display grid
  if (show_grid) {
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
  }

  std::for_each(g.arcs.begin(), g.arcs.end(), &draw_arc);
  std::for_each(g.nodes.begin(), g.nodes.end(), &draw_nodes);

  // Open context menu
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) ||
        !ImGui::IsAnyItemHovered()) {
      node_selected = Symbol(-1);
      open_context_menu = true;
    }
  if (open_context_menu) {
    ImGui::OpenPopup("context_menu");
    // if (node_hovered_in_list != Symbol(-1)) {
    // node_selected = node_hovered_in_list;
    // node_hovered_in_scene = node_hovered_in_list;
    // }
    // if (node_hovered_in_scene != Symbol(-1)) {
    //   // node_selected = node_hovered_in_scene;
    //   node_hovered_in_list = node_hovered_in_scene;
    // }
  }

  // Draw context menu
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
  if (ImGui::BeginPopup("context_menu")) {
    auto node =
        std::find_if(g.nodes.begin(), g.nodes.end(),
                     [=](const auto& n) { return n.id == node_selected; });
    ImVec2 scene_pos = ImGui::GetMousePosOnOpeningCurrentPopup() - offset;
    if (node != std::end(g.nodes)) {
      ImGui::Text("Node '%s'", node->name.c_str());
      ImGui::Separator();
      if (ImGui::MenuItem("Rename..", NULL, false, false)) {
      }
      if (ImGui::MenuItem("Delete", NULL, false, false)) {
      }
      if (ImGui::MenuItem("Copy", NULL, false, false)) {
      }
    } else {
      if (ImGui::MenuItem("Add place")) {
        MVC::push([&, scene_pos](Model&& m) {
          g.nodes.push_back(
              Node{"New node", Symbol('P', g.nodes.size()), scene_pos});
          return m;
        });
      }
      if (ImGui::MenuItem("Add transition")) {
        MVC::push([&, scene_pos](Model&& m) {
          g.nodes.push_back(
              Node{"New node", Symbol('T', g.nodes.size()), scene_pos});
          return m;
        });
      }
      if (ImGui::BeginMenu("Add arc")) {
        for (const auto& from : g.nodes) {
          if (ImGui::BeginMenu(from.name.c_str())) {
            for (const auto& to : g.nodes) {
              if (from.id.chr() != to.id.chr() &&
                  ImGui::MenuItem(to.name.c_str())) {
                g.arcs.push_back(
                    Arc{symmetri::Color::Success, &from.Pos, &to.Pos});
              }
            }
            ImGui::EndMenu();
          }
        }
        ImGui::EndMenu();
      }

      if (ImGui::MenuItem("Paste", NULL, false, false)) {
      }
    }
    ImGui::EndPopup();
  }
  ImGui::PopStyleVar();

  // Scrolling
  if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive() &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
    scrolling = scrolling + ImGui::GetIO().MouseDelta;
  }

  ImGui::PopItemWidth();
  ImGui::EndChild();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();
  ImGui::EndGroup();
}
