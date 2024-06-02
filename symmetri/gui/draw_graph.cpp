#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "draw_graph.h"

#include <math.h>  // fmodf

#include <optional>
#include <string_view>

#include "color.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "rxdispatch.h"
#include "symmetri/colors.hpp"

// State
static ImVec2 size;
static ImVec2 offset;

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

template <class T>
bool isPartOfVector(std::vector<T> const& vec, T const& item) {
  T const* data = vec.data();
  return !(std::less<T const*>{}(&item, data) ||
           std::greater_equal<T const*>{}(&item, data + vec.size()));
};

void moveView(const ImVec2& d) {
  rxdispatch::push([=](model::Model&& m) mutable {
    m.data->scrolling += d;
    return m;
  });
}

void moveNode(bool is_place, size_t idx, const ImVec2& d) {
  rxdispatch::push([=](model::Model&& m) mutable {
    (is_place ? m.data->p_positions[idx] : m.data->t_positions[idx]) += d;
    return m;
  });
}

void addNode(bool is_place, ImVec2 pos) {
  rxdispatch::push([=](model::Model&& m) mutable {
    if (is_place) {
      m.data->net.place.push_back("place");
      m.data->p_positions.push_back(pos);
      m.data->p_view.push_back(m.data->net.place.size() - 1);
    } else {
      m.data->net.transition.push_back("transition");
      m.data->net.output_n.push_back({});
      m.data->net.input_n.push_back({});
      m.data->net.priority.push_back(0);
      m.data->t_positions.push_back(pos);
      m.data->t_view.push_back(m.data->net.transition.size() - 1);
    }
    return m;
  });
}

void removePlace(const std::string* ptr) {
  rxdispatch::push([=](model::Model&& m) mutable {
    auto idx = GetIndexFromRef(m.data->net.place, *ptr);
    m.data->p_view.erase(
        std::remove(m.data->p_view.begin(), m.data->p_view.end(), idx),
        m.data->p_view.end());
    return m;
  });
}

void removeTransition(const std::string* ptr) {
  rxdispatch::push([=](model::Model&& m) mutable {
    auto idx = GetIndexFromRef(m.data->net.transition, *ptr);
    m.data->t_view.erase(
        std::remove(m.data->t_view.begin(), m.data->t_view.end(), idx),
        m.data->t_view.end());
    return m;
  });
}

void updateTransitionName(const size_t idx, const std::string& d) {
  rxdispatch::push([=](model::Model&& m) mutable {
    m.data->net.transition[idx] = d;
    return m;
  });
}

void updateTransitionPriority(const size_t idx, const int8_t priority) {
  rxdispatch::push([=](model::Model&& m) mutable {
    m.data->net.priority[idx] = priority;
    return m;
  });
}
void updateArcColor(const symmetri::AugmentedToken* ptr,
                    const symmetri::Token color) {
  rxdispatch::push([=](model::Model&& m) mutable {
    const_cast<symmetri::AugmentedToken*>(ptr)->color = color;
    return m;
  });
}

void updatePlaceName(const size_t idx, const std::string& d) {
  rxdispatch::push([=](model::Model&& m) mutable {
    m.data->net.place[idx] = d;
    return m;
  });
}

void setContextMenuActive() {
  rxdispatch::push([](model::Model&& m) {
    m.data->context_menu_active = true;
    return m;
  });
}

void setContextMenuInactive() {
  rxdispatch::push([](model::Model&& m) {
    m.data->context_menu_active = false;
    return m;
  });
}

void setSelectedNode(const std::string& idx) {
  rxdispatch::push([ptr = &idx](model::Model&& m) {
    m.data->selected_node = ptr;
    m.data->selected_arc = nullptr;
    return m;
  });
};

void resetSelection() {
  rxdispatch::push([](model::Model&& m) {
    m.data->selected_node = nullptr;
    m.data->selected_arc = nullptr;
    return m;
  });
};

void setSelectedArc(const symmetri::AugmentedToken& ptr) {
  rxdispatch::push([ptr = &ptr](model::Model&& m) {
    m.data->selected_arc = ptr;
    m.data->selected_node = nullptr;
    return m;
  });
};

void renderNodeEntry(const std::string& name, size_t idx, bool selected) {
  ImGui::PushID(idx);
  if (ImGui::Selectable(name.c_str(), selected)) {
    setSelectedNode(name);
  }
  ImGui::PopID();
}

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
  const auto draw = [&](const symmetri::AugmentedToken& t, bool is_input) {
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
      setSelectedArc(t);
    }

    ImU32 imcolor =
        getColor(t.color) |
        ((ImU32)IM_F32_TO_INT8_SAT(vm.selected_arc == &t ? 1.0f : 0.65f))
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

  for (const auto& token : vm.net.input_n[t_idx]) {
    draw(token, true);
  }
  for (const auto& token : vm.net.output_n[t_idx]) {
    draw(token, false);
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
    setSelectedNode(name);
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
  ImVec2 WindowSize = ImGui::GetWindowSize();
  WindowSize.y -= 140.0f;
  // Draw a list of nodes on the left side
  ImGui::BeginChild("some", ImVec2(200, 0));
  ImGui::Text("Selected");
  ImGui::Separator();
  ImGui::BeginChild("selected_node", ImVec2(200, 0.1 * WindowSize.y));
  if (vm.selected_node != nullptr) {
    auto transition_ptr =
        std::find_if(vm.net.transition.begin(), vm.net.transition.end(),
                     [=](const auto& p) { return vm.selected_node == &p; });
    bool is_place = transition_ptr == vm.net.transition.end();
    const auto& constainer = is_place ? vm.net.place : vm.net.transition;
    const size_t idx = std::distance(
        constainer.begin(),
        is_place ? std::find_if(
                       constainer.begin(), constainer.end(),
                       [=](const auto& p) { return vm.selected_node == &p; })
                 : transition_ptr);

    ImGui::Text("Name");
    ImGui::SameLine();
    static int i = 0;
    const auto id = std::string("##") + std::to_string(i++);
    ImGui::PushItemWidth(-1);
    const auto& model_name = constainer[idx];
    ImGui::Text("%s", model_name.c_str());
    static const char* data = nullptr;
    static char view_name[128] = "";
    if (data != model_name.data()) {
      data = model_name.data();
      strcpy(view_name, model_name.c_str());
    } else if (std::string_view(view_name).compare(model_name)) {
      // check if name is correct correct...
      is_place ? updatePlaceName(idx, std::string(view_name))
               : updateTransitionName(idx, std::string(view_name));
    }
    ImGui::InputText("input text", view_name, 128);
    ImGui::PopItemWidth();
    static std::optional<std::pair<size_t, int>> local_priority = std::nullopt;
    if (not local_priority.has_value()) {
      local_priority =
          std::make_pair(idx, static_cast<int>(vm.net.priority[idx]));  // crash
    } else if (idx == local_priority->first &&
               local_priority->second != vm.net.priority[idx]) {
      updateTransitionPriority(idx, local_priority->second);
    } else if (idx != local_priority->first) {
      local_priority.reset();
    }

    if (not is_place && local_priority.has_value()) {
      ImGui::Text("Priority");
      ImGui::SameLine();
      ImGui::InputInt("##", &(local_priority->second));
    }
  } else if (vm.selected_arc != nullptr) {
    static std::optional<
        std::pair<const symmetri::AugmentedToken*, symmetri::Token>>
        local_color = std::nullopt;
    if (not local_color.has_value()) {
      local_color = {vm.selected_arc, vm.selected_arc->color};
    } else if (local_color->first != nullptr &&
               vm.selected_arc == local_color->first &&
               local_color->second != vm.selected_arc->color) {
      // if statement also buggy
      updateArcColor(vm.selected_arc, local_color->second);
      local_color = std::nullopt;
    } else if (local_color->first == nullptr ||
               vm.selected_arc != local_color->first) {
      local_color.reset();
    }

    if (local_color.has_value() &&
        ImGui::BeginMenu(
            symmetri::Color::toString(vm.selected_arc->color).c_str())) {
      for (const auto& color : vm.colors) {
        if (ImGui::MenuItem(color.c_str())) {
          local_color = {vm.selected_arc,
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
    renderNodeEntry(vm.net.place[idx], idx,
                    &vm.net.place[idx] == vm.selected_node);
  }
  ImGui::EndChild();
  ImGui::Dummy(ImVec2(0.0f, 20.0f));
  ImGui::Text("Transitions");
  ImGui::Separator();
  ImGui::BeginChild("transition_list",
                    ImVec2(200, height_fraction * WindowSize.y));
  for (const auto& idx : vm.t_view) {
    renderNodeEntry(vm.net.transition[idx], idx,
                    &vm.net.transition[idx] == vm.selected_node);
  }

  ImGui::EndChild();
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginGroup();

  // Create our child canvas
  ImGui::Text("Hold middle mouse button to scroll (%.2f,%.2f)", vm.scrolling.x,
              vm.scrolling.y);
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
               &vm.net.transition[idx] == vm.selected_node);
  }
  for (auto&& idx : vm.p_view) {
    draw_nodes(true, idx, vm.net.place[idx], vm.p_positions[idx],
               &vm.net.place[idx] == vm.selected_node);
  }

  // Open context menu
  if (not vm.context_menu_active &&
      ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) ||
        !ImGui::IsAnyItemHovered()) {
      // if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup))
      // {
      setContextMenuActive();
      // }
    }
  }

  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
      !ImGui::IsAnyItemHovered()) {
    setContextMenuInactive();
    if (vm.selected_node != nullptr || vm.selected_arc != nullptr) {
      resetSelection();
    }
  }

  if (vm.context_menu_active) {
    ImGui::OpenPopup("context_menu");
  }

  // Draw context menu
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
  if (ImGui::BeginPopup("context_menu")) {
    if (vm.selected_node) {
      ImGui::Text("Node '%s'", vm.selected_node->c_str());
      ImGui::Separator();
      if (ImGui::MenuItem("Delete")) {
        auto transition_ptr =
            std::find_if(vm.net.transition.begin(), vm.net.transition.end(),
                         [=](const auto& p) { return vm.selected_node == &p; });

        bool is_place = transition_ptr == vm.net.transition.end();
        is_place ? removePlace(vm.selected_node)
                 : removeTransition(vm.selected_node);
      }
    } else if (vm.selected_arc) {
      ImGui::Text("Arc");
      ImGui::Separator();
      if (ImGui::MenuItem("Delete")) {
      }
      if (ImGui::BeginMenu("Change color")) {
        for (const auto& color : vm.colors) {
          if (ImGui::MenuItem(color.c_str())) {
          }
        }
        ImGui::EndMenu();
      }
    } else {
      ImVec2 scene_pos = ImGui::GetMousePosOnOpeningCurrentPopup() - offset;
      if (ImGui::MenuItem("Add place")) {
        addNode(true, scene_pos);
      }
      if (ImGui::MenuItem("Add transition")) {
        addNode(false, scene_pos);
      }
      if (ImGui::MenuItem("Add arc")) {
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
