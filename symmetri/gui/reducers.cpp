#include "gui/model.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include <stdint.h>

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <optional>
#include <ranges>
#include <tuple>
#include <vector>

#include "draw_context_menu.h"
#include "imgui_internal.h"
#include "petri.h"
#include "reducers.h"
#include "rxdispatch.h"
#include "small_vector.hpp"
#include "symmetri/callback.h"

namespace symmetri {
struct DirectMutation;
}  // namespace symmetri

void moveView(const ImVec2& d) {
  rxdispatch::pushView([=](model::ViewState& v, const model::EditState&) {
    v.scrolling.x += d.x / v.zoom_factor;
    v.scrolling.y += d.y / v.zoom_factor;
  });
}

void moveNode(model::Model::NodeType node_type, size_t idx, const ImVec2& d) {
  // `d` is a world-space delta (the caller divides the screen delta by the
  // zoom factor); edit reducers must not read view state such as the zoom.
  rxdispatch::pushEdit([=](model::EditState&& e) {
    (model::Model::NodeType::Place == node_type ? e.p_positions[idx]
                                                : e.t_positions[idx]) +=
        model::Coordinate{d.x, d.y};
    return e;
  });
}

void showGrid(bool show_grid) {
  rxdispatch::pushView([=](model::ViewState& v, const model::EditState&) {
    v.show_grid = show_grid;
  });
}

std::string viewContainsNameAlready(const std::vector<size_t>& view,
                                    const std::vector<std::string>& names,
                                    std::string&& name, size_t j = 0) {
  for (size_t i : view) {
    if (names[i] == name) {
      return viewContainsNameAlready(
          view, names, std::move(name) + "_" + std::to_string(j), j + 1);
    }
  }
  return name;
}

void setArcHoverState(bool is_an_arc_hovered) {
  rxdispatch::pushView([=](model::ViewState& v, const model::EditState&) {
    v.arc_hovered = is_an_arc_hovered;
  });
}

void addNode(model::Model::NodeType node_type, ImVec2 pos) {
  rxdispatch::pushEdit([=](model::EditState&& e) {
    if (model::Model::NodeType::Place == node_type) {
      e.net.place.push_back(
          viewContainsNameAlready(e.p_view, e.net.place, "place"));
      e.net.p_to_ts_n.push_back({});
      e.p_positions.push_back(model::Coordinate{pos.x, pos.y});
      e.p_view.push_back(e.net.place.size() - 1);
    } else {
      e.net.transition.push_back(
          viewContainsNameAlready(e.t_view, e.net.transition, "transition"));
      e.net.output_n.push_back({});
      e.net.input_n.push_back({});
      e.net.priority.push_back(0);
      e.net.store.emplace_back(symmetri::identity<symmetri::DirectMutation>{});
      e.t_positions.push_back(model::Coordinate{pos.x, pos.y});
      e.t_view.push_back(e.net.transition.size() - 1);
    }
    return e;
  });
  setContextMenuInactive();
}

void removeArc(model::Model::NodeType source_node_type, size_t transition_idx,
               size_t sub_idx) {
  rxdispatch::pushEdit([=](model::EditState&& e) {
    // remove transition from view
    e.t_view.erase(
        std::remove(e.t_view.begin(), e.t_view.end(), transition_idx),
        e.t_view.end());

    const size_t new_idx = e.net.transition.size();
    // add it again to the view...
    e.net.transition.push_back(e.net.transition[transition_idx]);
    e.net.store.emplace_back(symmetri::identity<symmetri::DirectMutation>{});
    e.net.priority.push_back(e.net.priority[transition_idx]);
    e.t_positions.push_back(e.t_positions[transition_idx]);
    e.t_view.push_back(new_idx);

    // now copy the arcs, except for the one we want to delete
    if (source_node_type == model::Model::NodeType::Place) {
      e.net.output_n.push_back(e.net.output_n[transition_idx]);
      e.net.input_n.push_back({});
      std::copy_if(e.net.input_n[transition_idx].begin(),
                   e.net.input_n[transition_idx].end(),
                   std::back_inserter(e.net.input_n.back()),
                   [=, i = size_t(0)](auto) mutable { return i++ != sub_idx; });
    } else {
      e.net.input_n.push_back(e.net.input_n[transition_idx]);
      e.net.output_n.push_back({});
      std::copy_if(e.net.output_n[transition_idx].begin(),
                   e.net.output_n[transition_idx].end(),
                   std::back_inserter(e.net.output_n.back()),
                   [=, i = size_t(0)](auto) mutable { return i++ != sub_idx; });
    }
    return e;
  });
  rxdispatch::pushView([](model::ViewState& v, const model::EditState&) {
    std::erase(v.drawables, &draw_context_menu);
  });
}

void addArc(model::Model::NodeType source_node_type, size_t source,
            size_t target, symmetri::Token color) {
  rxdispatch::pushEdit([=](model::EditState&& e) {
    const size_t transition_idx =
        model::Model::NodeType::Transition == source_node_type ? source
                                                               : target;
    // remove transition from view
    e.t_view.erase(
        std::remove(e.t_view.begin(), e.t_view.end(), transition_idx),
        e.t_view.end());
    // add it again to the view...
    const size_t new_transition_idx = e.net.transition.size();
    e.net.transition.push_back(e.net.transition[transition_idx]);
    e.net.output_n.push_back(e.net.output_n[transition_idx]);
    e.net.input_n.push_back(e.net.input_n[transition_idx]);
    e.net.priority.push_back(e.net.priority[transition_idx]);
    e.net.store.emplace_back(symmetri::identity<symmetri::DirectMutation>{});

    e.t_positions.push_back(e.t_positions[transition_idx]);
    e.t_view.push_back(new_transition_idx);

    // add the arc
    auto& p_to_ts = e.net.p_to_ts_n[source];
    switch (source_node_type) {
      case model::Model::NodeType::Place:
        e.net.input_n[new_transition_idx].push_back({source, color});
        if (std::find(p_to_ts.begin(), p_to_ts.end(), transition_idx) ==
            p_to_ts.end()) {
          p_to_ts.push_back(new_transition_idx);
        }
        break;
      case model::Model::NodeType::Transition:
        e.net.output_n[new_transition_idx].push_back({target, color});
        break;
    };
    return e;
  });
  rxdispatch::pushView([](model::ViewState& v, const model::EditState&) {
    std::erase(v.drawables, &draw_context_menu);
  });
}

void removePlace(size_t idx) {
  rxdispatch::pushEdit([=](model::EditState&& e) {
    // remove node from view
    e.p_view.erase(std::remove(e.p_view.begin(), e.p_view.end(), idx),
                   e.p_view.end());
    // remove marking for this node
    auto [b, en] = std::ranges::remove_if(
        e.tokens, [idx](const symmetri::AugmentedToken at) {
          return std::get<size_t>(at) == idx;
        });
    e.tokens.erase(b, en);
    return e;
  });
  rxdispatch::pushView([=](model::ViewState& v, const model::EditState&) {
    std::erase(v.p_highlight, idx);
    std::erase(v.drawables, &draw_context_menu);
  });
}

void removeTransition(size_t idx) {
  rxdispatch::pushEdit([=](model::EditState&& e) {
    e.t_view.erase(std::remove(e.t_view.begin(), e.t_view.end(), idx),
                   e.t_view.end());
    return e;
  });
  rxdispatch::pushView([=](model::ViewState& v, const model::EditState&) {
    std::erase(v.t_highlight, idx);
    std::erase(v.drawables, &draw_context_menu);
  });
}

void updateArcColor(model::Model::NodeType source_node_type, size_t idx,
                    size_t sub_idx, const symmetri::Token color) {
  rxdispatch::pushEdit([=](model::EditState&& e) {
    std::get<symmetri::Token>((source_node_type == model::Model::NodeType::Place
                                   ? e.net.input_n
                                   : e.net.output_n)[idx][sub_idx]) = color;
    return e;
  });
}

int updatePriority(ImGuiInputTextCallbackData* data) {
  size_t idx = *static_cast<size_t*>(data->UserData);
  data->ClearSelection();
  if ((data->Buf != NULL) && (data->Buf[0] == '\0')) {
  } else if (strcmp(data->Buf, "-") == 0) {
  } else {
    const auto priority = std::clamp(std::stoi(data->Buf), INT8_MIN, INT8_MAX);
    rxdispatch::pushEdit([=](model::EditState&& e) {
      e.net.priority[idx] = priority;
      return e;
    });
  }
  return 0;
}

int updatePlaceName(ImGuiInputTextCallbackData* data) {
  size_t idx = *static_cast<size_t*>(data->UserData);
  data->ClearSelection();
  rxdispatch::pushEdit(
      [=, name = std::string(data->Buf)](model::EditState&& e) {
        e.net.place[idx] = name;
        return e;
      });
  return 0;
}

int updateTransitionName(ImGuiInputTextCallbackData* data) {
  // we add 1000 to distinguish from places.. hsould be neater someday.
  size_t idx = (*static_cast<size_t*>(data->UserData)) - 1000;
  data->ClearSelection();

  rxdispatch::pushEdit(
      [=, name = std::string(data->Buf)](model::EditState&& e) {
        e.net.transition[idx] = name;
        return e;
      });
  return 0;
}

void setContextMenuActive(ImVec2 world_pos) {
  rxdispatch::pushView([=](model::ViewState& v, const model::EditState&) {
    v.drawables.push_back(&draw_context_menu);
    v.context_menu_pos = {world_pos.x, world_pos.y};
  });
}

void setContextMenuInactive() {
  rxdispatch::pushView([](model::ViewState& v, const model::EditState&) {
    std::erase(v.drawables, &draw_context_menu);
    v.context_menu_pos.reset();
  });
}

void setSelectedNode(model::Model::NodeType node_type, size_t idx) {
  rxdispatch::pushView([=](model::ViewState& v, const model::EditState&) {
    v.selected_node_idx = {node_type, idx};
    v.p_highlight.clear();
    v.t_highlight.clear();
    v.arc_highlight.clear();
    v.selected_arc_idxs.reset();
    (node_type == model::Model::NodeType::Place ? v.p_highlight : v.t_highlight)
        .push_back(idx);
  });
};

void addHighlightNode(model::Model::NodeType node_type, size_t idx) {
  rxdispatch::pushView([=](model::ViewState& v, const model::EditState&) {
    (node_type == model::Model::NodeType::Place ? v.p_highlight : v.t_highlight)
        .push_back(idx);
  });
};

void setSelectedTargetNode(model::Model::NodeType node_type, size_t idx) {
  rxdispatch::pushView([=](model::ViewState& v, const model::EditState&) {
    auto& c = (node_type == model::Model::NodeType::Place ? v.p_highlight
                                                          : v.t_highlight);
    c.clear();
    c.push_back(idx);
  });
};

void resetSelection() {
  rxdispatch::pushView([](model::ViewState& v, const model::EditState&) {
    v.selected_node_idx.reset();
    v.p_highlight.clear();
    v.t_highlight.clear();
    v.selected_arc_idxs.reset();
    v.arc_highlight.clear();
  });
};

void addHighlightArc(model::Model::NodeType source_node_type, size_t t_idx,
                     size_t sub_idx) {
  rxdispatch::pushView([=](model::ViewState& v, const model::EditState&) {
    v.arc_highlight.emplace_back(source_node_type, t_idx, sub_idx);
  });
};

void setSelectedArc(model::Model::NodeType source_node_type, size_t t_idx,
                    size_t sub_idx) {
  rxdispatch::pushView([=](model::ViewState& v, const model::EditState&) {
    v.selected_node_idx.reset();
    v.p_highlight.clear();
    v.t_highlight.clear();
    v.arc_highlight.clear();
    v.selected_arc_idxs = {source_node_type, t_idx, sub_idx};
    v.arc_highlight.emplace_back(source_node_type, t_idx, sub_idx);
  });
};

void renderNodeEntry(model::Model::NodeType node_type, const std::string& name,
                     size_t idx, bool selected) {
  ImGui::PushID(idx);
  if (ImGui::Selectable(name.c_str(), selected)) {
    setSelectedNode(node_type, idx);
  }
  ImGui::PopID();
}

int updateActiveFile(ImGuiInputTextCallbackData* data) {
  data->ClearSelection();
  if (data->Buf != nullptr) {
    rxdispatch::pushEdit(
        [file_name = std::string(data->Buf)](model::EditState&& e) {
          e.active_file = std::filesystem::path(file_name);
          return e;
        });
  }
  return 0;
}

void updateColorTable() {
  rxdispatch::pushView([](model::ViewState& v, const model::EditState&) {
    v.colors = symmetri::Token::getColors();
  });
}

void addTokenToPlace(symmetri::AugmentedToken token) {
  rxdispatch::pushEdit([=](model::EditState&& e) {
    e.tokens.push_back(token);
    return e;
  });
  rxdispatch::pushView([](model::ViewState& v, const model::EditState&) {
    std::erase(v.drawables, &draw_context_menu);
  });
}

void removeTokenFromPlace(symmetri::AugmentedToken token) {
  rxdispatch::pushEdit([=](model::EditState&& e) {
    if (auto it = std::ranges::find(e.tokens, token); it != e.tokens.end()) {
      e.tokens.erase(it);
    }
    return e;
  });
}

void resetSelectedTargetNode() {
  rxdispatch::pushView([](model::ViewState& v, const model::EditState&) {
    v.selected_arc_idxs.reset();
  });
}

void zoomRelative(float f) {
  rxdispatch::pushView([=](model::ViewState& v, const model::EditState&) {
    v.zoom_factor += f;
  });
}

void zoomAbsolute(float f) {
  rxdispatch::pushView(
      [=](model::ViewState& v, const model::EditState&) { v.zoom_factor = f; });
}

void resetNetView() {
  rxdispatch::pushView([](model::ViewState& v, const model::EditState& e) {
    if (e.t_view.empty() || e.p_view.empty()) return;
    const auto getMinX = [&](const auto& in_view, const auto& pos) {
      return std::ranges::min(in_view | std::views::transform([&](auto idx) {
                                return pos[idx].x;
                              }));
    };
    const auto getMinY = [&](const auto& in_view, const auto& pos) {
      return std::ranges::min(in_view | std::views::transform([&](auto idx) {
                                return pos[idx].y;
                              }));
    };

    float min_x = std::min(getMinX(e.t_view, e.t_positions),
                           getMinX(e.p_view, e.p_positions));
    float min_y = std::min(getMinY(e.t_view, e.t_positions),
                           getMinY(e.p_view, e.p_positions));

    v.scrolling = {400 - min_x, 120 - min_y};
  });
}

void tryFire(size_t transition_idx) {
  // Capture the timestamp at creation so replaying this edit reducer during
  // undo/redo reproduces the same log entry deterministically.
  rxdispatch::pushEdit([=, now = symmetri::Clock::now()](model::EditState&& e) {
    if (e.net.input_n[transition_idx].empty() ||
        canFire(e.net.input_n[transition_idx], e.tokens)) {
      // deduct
      deductMarking(e.tokens, e.net.input_n[transition_idx]);
      // add
      const auto& lookup_t = e.net.output_n[transition_idx];
      const auto result = fire(e.net.store[transition_idx]);
      for (const auto& [p, c] : lookup_t) {
        e.tokens.emplace_back(p, result);
      }
      e.log.push_back({transition_idx, result, now});
    }
    return e;
  });
}
void updateTransitionOutputColor(size_t transition_idx, symmetri::Token color) {
  rxdispatch::pushEdit([=](model::EditState&& e) {
    e.net.store[transition_idx] = [=] { return color; };
    return e;
  });
}
