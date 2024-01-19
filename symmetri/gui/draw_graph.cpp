#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include <math.h>  // fmodf

#include <cmath>
#include <iostream>

#include "color.hpp"
#include "graph.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "rxdispatch.h"
#include "symmetri/colors.hpp"

// State
static ImVec2 scrolling = ImVec2(0.0f, 0.0f);
static bool show_grid = true;
static ImVec2 size;
static ImVec2 offset;
static ImDrawList* draw_list;
static bool open_context_menu = false;
static const Node* node_selected = nullptr;
static const Node* node_hovered_in_list = nullptr;
static const Node* node_hovered_in_scene = nullptr;
static const Arc* arc_selected = nullptr;
static const Arc* arc_hovered_in_scene = nullptr;

static const float NODE_SLOT_RADIUS = 4.0f;
static const ImVec2 NODE_WINDOW_PADDING(8.0f, 8.0f);

template <class T>
std::size_t GetIndexFromRef(std::vector<T> const& vec, T const& item) {
  T const* data = vec.data();

  if (std::less<T const*>{}(&item, data) ||
      std::greater_equal<T const*>{}(&item, data + vec.size()))
    throw std::out_of_range{"The given object is not part of the vector."};

  return static_cast<std::size_t>(&item - vec.data());
};

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

void draw_grid() {
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

void draw_arc(const Arc& arc, const std::vector<Node>& nodes) {
  const auto& [color, from_to_idx] = arc;
  ImVec2 p1 = offset + Node::GetCenterPos(nodes[from_to_idx[0]].Pos, size);
  ImVec2 p2 = offset + Node::GetCenterPos(nodes[from_to_idx[1]].Pos, size);

  // use with:
  using namespace ImGui;
  const float max_distance = 2.f;
  const auto mouse_pos = ImGui::GetIO().MousePos;
  ImVec2 mouse_pos_projected_on_segment = ImLineClosestPoint(p1, p2, mouse_pos);
  ImVec2 mouse_pos_delta_to_segment =
      mouse_pos_projected_on_segment - mouse_pos;
  bool is_segment_hovered =
      arc_selected == &arc ||
      (ImLengthSqr(mouse_pos_delta_to_segment) <= max_distance * max_distance &&
       node_hovered_in_scene == nullptr);

  if (is_segment_hovered) {
    arc_hovered_in_scene = &arc;
  } else if (arc_hovered_in_scene == &arc) {
    arc_hovered_in_scene = nullptr;
  }

  if (is_segment_hovered && ImGui::IsMouseClicked(0)) {
    arc_selected = &arc;
    node_selected = nullptr;
  }

  ImU32 imcolor =
      getColor(color) |
      ((ImU32)IM_F32_TO_INT8_SAT(arc_hovered_in_scene == &arc ? 1.0f : 0.65f))
          << IM_COL32_A_SHIFT;

  const auto d = p2 - p1;
  const auto theta = std::atan2(d.y, d.x) - M_PI_2;
  const float h = draw_list->_Data->FontSize * 1.00f;
  const float r = h * 0.40f * 1;
  const ImVec2 center = p1 + d * 0.6f;
  const auto a_sin = std::sin(theta);
  const auto a_cos = std::cos(theta);
  const auto a = ImRotate(ImVec2(+0.000f, +1.f) * r, a_cos, a_sin);
  const auto b = ImRotate(ImVec2(-1.f, -1.f) * r, a_cos, a_sin);
  const auto c = ImRotate(ImVec2(+1.f, -1.f) * r, a_cos, a_sin);
  draw_list->AddTriangleFilled(center + a, center + b, center + c, imcolor);
  draw_list->AddLine(p1, p2, imcolor,
                     arc_hovered_in_scene == &arc ? 3.0f : 2.0f);
};

void draw_nodes(Node& node) {
  open_context_menu = false;
  ImGui::PushID(node.name.c_str());
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
    node_selected = &node;
    arc_selected = nullptr;
  }
  if (node_moving_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    rxdispatch::push([Pos = &node.Pos,
                      d = ImGui::GetIO().MouseDelta](model::Model& m) mutable {
      *Pos += d;
      return m;
    });
  }
  const int opacity = 255;
  auto select_color =
      &node == node_selected ? IM_COL32(255, 255, 0, opacity)
      : &node == node_hovered_in_list || &node == node_hovered_in_scene
          ? IM_COL32(0, 255, 0, opacity)
          : IM_COL32(100, 100, 100, opacity);
  if (node.type == Node::Type::Place) {
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
void draw(Graph& g) {
  ImGui::Begin("test", NULL, ImGuiWindowFlags_NoTitleBar);

  ImVec2 WindowSize = ImGui::GetWindowSize();
  WindowSize.y -= 140.0f;
  // Draw a list of nodes on the left side
  ImGui::BeginChild("some", ImVec2(200, 0));
  ImGui::Text("Selected");
  ImGui::Separator();
  ImGui::BeginChild("selected_node", ImVec2(200, 0.1 * WindowSize.y));
  if (node_selected) {
    auto node =
        std::find_if(g.nodes.begin(), g.nodes.end(),
                     [=](const auto& n) { return &n == node_selected; });
    if (node != std::end(g.nodes)) {
      ImGui::Text("Name");
      ImGui::SameLine();
      static int i = 0;
      const auto id = std::string("##") + std::to_string(i++);
      ImGui::PushItemWidth(-1);
      ImGui::InputText(id.c_str(), node->name.data(), 30);
      ImGui::PopItemWidth();
      static int priority = 1;
      static int multiplicity = 1;
      if (node_selected->type == Node::Type::Transition) {
        ImGui::Text("Priority");
        ImGui::SameLine();
        ImGui::InputInt("##", &priority);
        ImGui::Text("Weight");
        ImGui::SameLine();
        ImGui::InputInt("##", &multiplicity);
      }
    }

  } else if (arc_selected) {
    const Node& from = g.nodes[arc_selected->from_to_pos_idx[0]];
    const Node& to = g.nodes[arc_selected->from_to_pos_idx[1]];
    ImGui::Text("From:");
    ImGui::SameLine();
    ImGui::PushItemWidth(-1);
    ImGui::Text("%s", from.name.data());
    ImGui::PopItemWidth();
    ImGui::Text("To:");
    ImGui::SameLine();
    ImGui::PushItemWidth(-1);
    ImGui::Text("%s", to.name.data());
    ImGui::PopItemWidth();
    ImGui::Text("Color");
    ImGui::SameLine();
    ImGui::Text("%s", symmetri::Color::toString(arc_selected->color).c_str());
  }
  ImGui::EndChild();

  ImGui::Dummy(ImVec2(0.0f, 20.0f));

  ImGui::Text("Places");
  ImGui::Separator();
  constexpr float height_fraction = 0.8 / 2.0;
  ImGui::BeginChild("place_list", ImVec2(200, height_fraction * WindowSize.y));
  for (auto idx : g.n_idx) {
    const auto& node = g.nodes[idx];
    if (node.type == Node::Type::Place) {
      ImGui::PushID(idx);
      if (ImGui::Selectable(node.name.c_str(), &node == node_selected)) {
        node_selected = &node;
        arc_selected = nullptr;
      }
      node_hovered_in_list = ImGui::IsItemHovered()      ? &node
                             : ImGui::IsAnyItemHovered() ? node_hovered_in_list
                                                         : nullptr;

      ImGui::PopID();
    }
  }
  ImGui::EndChild();

  ImGui::Dummy(ImVec2(0.0f, 20.0f));
  ImGui::Text("Transitions");
  ImGui::Separator();
  ImGui::BeginChild("transition_list",
                    ImVec2(200, height_fraction * WindowSize.y));
  for (auto idx : g.n_idx) {
    const auto& node = g.nodes[idx];
    if (node.type == Node::Type::Transition) {
      ImGui::PushID(idx);
      if (ImGui::Selectable(node.name.c_str(), &node == node_selected)) {
        node_selected = &node;
        arc_selected = nullptr;
      }
      node_hovered_in_list = ImGui::IsItemHovered()      ? &node
                             : ImGui::IsAnyItemHovered() ? node_hovered_in_list
                                                         : nullptr;
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
  ImGui::SameLine();
  const auto io = ImGui::GetIO();
  ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
              1000.0f / io.Framerate, io.Framerate);
  ImGui::SameLine(ImGui::GetWindowWidth() - 340);
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
    draw_grid();
  }

  // draw arcs
  int i = 0;
  for (const auto& idx : g.a_idx) {
    draw_arc(g.arcs[idx], g.nodes);
  }

  // draw places & transitions
  for (auto idx : g.n_idx) {
    draw_nodes(g.nodes[idx]);
  }

  // Open context menu
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) ||
        !ImGui::IsAnyItemHovered()) {
      node_selected = node_hovered_in_scene;
      arc_selected = arc_hovered_in_scene;
      open_context_menu = true;
    } else {
      node_selected = nullptr;
      arc_selected = nullptr;
    }
  }

  if (open_context_menu) {
    ImGui::OpenPopup("context_menu");
  }

  // Draw context menu
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
  if (ImGui::BeginPopup("context_menu")) {
    if (node_selected) {
      ImGui::Text("Node '%s'", node_selected->name.c_str());
      ImGui::Separator();
      if (ImGui::MenuItem("Delete")) {
        rxdispatch::push([node_selected = node_selected](model::Model& m_ptr) {
          auto& m = *m_ptr.data;
          const auto idx = std::distance(
              m.graph.nodes.begin(),
              std::find_if(m.graph.nodes.begin(), m.graph.nodes.end(),
                           [=](const Node& n) { return node_selected == &n; }));
          const auto swap_idx = std::distance(
              m.graph.n_idx.begin(),
              std::find(m.graph.n_idx.begin(), m.graph.n_idx.end(), idx));
          std::swap(m.graph.n_idx[swap_idx], m.graph.n_idx.back());
          m.graph.n_idx.pop_back();

          // delete arcs related to this
          for (auto iter = m.graph.a_idx.begin();
               iter != m.graph.a_idx.end();) {
            const auto [color, from_to_idx] = m.graph.arcs[*iter];
            if (from_to_idx[0] == idx || from_to_idx[1] == idx) {
              m.graph.a_idx.erase(iter);
            } else {
              ++iter;
            }
          }
          return m_ptr;
        });
        node_selected = nullptr;
        // node_hovered_in_list = nullptr;
        // node_hovered_in_scene = nullptr;
        // arc_selected = nullptr;
        // arc_hovered_in_scene = nullptr;
      }
    } else if (arc_selected) {
      ImGui::Text("Arc");
      ImGui::Separator();

      if (ImGui::MenuItem("Delete")) {
        rxdispatch::push([&, ptr = arc_selected](model::Model& m_ptr) {
          auto& m = *m_ptr.data;
          const auto idx = std::distance(
              g.arcs.begin(),
              std::find_if(g.arcs.begin(), g.arcs.end(),
                           [=](const Arc& a) { return ptr == &a; }));
          const auto swap_idx = std::distance(
              g.a_idx.begin(), std::find(g.a_idx.begin(), g.a_idx.end(), idx));
          std::swap(g.a_idx[swap_idx], g.a_idx.back());
          g.a_idx.pop_back();
          return m_ptr;
        });
        arc_selected = nullptr;
      }
      if (ImGui::BeginMenu("Change color")) {
        for (const auto& arc : symmetri::Color::getColors()) {
          if (ImGui::MenuItem(arc.second.c_str())) {
            rxdispatch::push([color = arc.first,
                              ptr = arc_selected](model::Model& m_ptr) {
              auto& m = *m_ptr.data;
              const auto idx = std::distance(
                  m.graph.arcs.begin(),
                  std::find_if(m.graph.arcs.begin(), m.graph.arcs.end(),
                               [=](const Arc& a) { return ptr == &a; }));
              const auto swap_idx = std::distance(
                  m.graph.a_idx.begin(),
                  std::find(m.graph.a_idx.begin(), m.graph.a_idx.end(), idx));
              std::swap(m.graph.a_idx[swap_idx], m.graph.a_idx.back());
              m.graph.a_idx.pop_back();
              m.graph.a_idx.push_back(m.graph.arcs.size());
              m.graph.arcs.push_back({color, ptr->from_to_pos_idx});
              return m_ptr;
            });
            arc_selected = nullptr;
          }
        }
        ImGui::EndMenu();
      }
    } else {
      ImVec2 scene_pos = ImGui::GetMousePosOnOpeningCurrentPopup() - offset;
      if (ImGui::MenuItem("Add place")) {
        rxdispatch::push([scene_pos](model::Model& m_ptr) {
          auto& m = *m_ptr.data;
          m.graph.n_idx.push_back(m.graph.nodes.size());
          m.graph.nodes.push_back(
              Node{"New node", Node::Type::Place, scene_pos});
          return m_ptr;
        });
      }
      if (ImGui::MenuItem("Add transition")) {
        rxdispatch::push([scene_pos](model::Model& m_ptr) {
          auto& m = *m_ptr.data;
          m.graph.n_idx.push_back(m.graph.nodes.size());
          m.graph.nodes.push_back(
              Node{"New node", Node::Type::Transition, scene_pos});
          return m_ptr;
        });
      }
      if (ImGui::BeginMenu("Add arc")) {
        for (const auto& from_idx : g.n_idx) {
          const auto& from = g.nodes[from_idx];
          if (ImGui::BeginMenu(from.name.c_str())) {
            node_hovered_in_scene = &from;
            for (const auto& to_idx : g.n_idx) {
              const auto& to = g.nodes[to_idx];
              if (from.type != to.type && ImGui::BeginMenu(to.name.c_str())) {
                node_hovered_in_scene = &to;
                for (const auto& color : symmetri::Color::getColors()) {
                  if (ImGui::MenuItem(color.second.c_str())) {
                    rxdispatch::push(
                        [arc = Arc{color.first, {from_idx, to_idx}}](
                            model::Model& m_ptr) {
                          auto& m = *m_ptr.data;
                          m.graph.a_idx.push_back(m.graph.arcs.size());
                          m.graph.arcs.push_back(arc);
                          return m_ptr;
                        });
                    node_hovered_in_scene = nullptr;
                  }
                }
                ImGui::EndMenu();
              }
            }
            ImGui::EndMenu();
          }
        }
        ImGui::EndMenu();
      }
    }
    ImGui::EndPopup();
  }
  ImGui::PopStyleVar();

  // Scrolling
  if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive() &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f)) {
    scrolling = scrolling + ImGui::GetIO().MouseDelta;
  }

  ImGui::PopItemWidth();
  ImGui::EndChild();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();
  ImGui::EndGroup();
}
