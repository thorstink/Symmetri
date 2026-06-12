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
  rxdispatch::push([=](model::Model&& m) mutable {
    m.scrolling.x += d.x / m.zoom_factor;
    m.scrolling.y += d.y / m.zoom_factor;
    return m;
  });
}

void moveNode(model::Model::NodeType node_type, size_t idx, const ImVec2& d) {
  rxdispatch::push([=](model::Model&& m) mutable {
    (model::Model::NodeType::Place == node_type ? m.p_positions[idx]
                                                : m.t_positions[idx]) +=
        model::Coordinate{d.x / m.zoom_factor, d.y / m.zoom_factor};
    return m;
  });
}

void showGrid(bool show_grid) {
  rxdispatch::push([=](model::Model&& m) mutable {
    m.show_grid = show_grid;
    return m;
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
  rxdispatch::push([=](model::Model&& m) mutable {
    m.arc_hovered = is_an_arc_hovered;
    return m;
  });
}

void addNode(model::Model::NodeType node_type, ImVec2 pos) {
  rxdispatch::push([=](model::Model&& m) mutable {
    if (model::Model::NodeType::Place == node_type) {
      m.net.place.push_back(
          viewContainsNameAlready(m.p_view, m.net.place, "place"));
      m.net.p_to_ts_n.push_back({});
      m.p_positions.push_back(model::Coordinate{pos.x, pos.y});
      m.p_view.push_back(m.net.place.size() - 1);
    } else {
      m.net.transition.push_back(
          viewContainsNameAlready(m.t_view, m.net.transition, "transition"));
      m.net.output_n.push_back({});
      m.net.input_n.push_back({});
      m.net.priority.push_back(0);
      m.net.store.emplace_back(symmetri::identity<symmetri::DirectMutation>{});
      m.t_positions.push_back(model::Coordinate{pos.x, pos.y});
      m.t_view.push_back(m.net.transition.size() - 1);
    }
    setContextMenuInactive();
    return m;
  });
}

void removeArc(model::Model::NodeType source_node_type, size_t transition_idx,
               size_t sub_idx) {
  rxdispatch::push([=](model::Model&& m) mutable {
    // remove transition from view
    m.t_view.erase(
        std::remove(m.t_view.begin(), m.t_view.end(), transition_idx),
        m.t_view.end());

    const size_t new_idx = m.net.transition.size();
    // add it again to the view...
    m.net.transition.push_back(m.net.transition[transition_idx]);
    m.net.store.emplace_back(symmetri::identity<symmetri::DirectMutation>{});
    m.net.priority.push_back(m.net.priority[transition_idx]);
    m.t_positions.push_back(m.t_positions[transition_idx]);
    m.t_view.push_back(new_idx);

    // now copy the arcs, except for the one we want to delete
    if (source_node_type == model::Model::NodeType::Place) {
      m.net.output_n.push_back(m.net.output_n[transition_idx]);
      m.net.input_n.push_back({});
      std::copy_if(m.net.input_n[transition_idx].begin(),
                   m.net.input_n[transition_idx].end(),
                   std::back_inserter(m.net.input_n.back()),
                   [=, i = size_t(0)](auto) mutable { return i++ != sub_idx; });
    } else {
      m.net.input_n.push_back(m.net.input_n[transition_idx]);
      m.net.output_n.push_back({});
      std::copy_if(m.net.output_n[transition_idx].begin(),
                   m.net.output_n[transition_idx].end(),
                   std::back_inserter(m.net.output_n.back()),
                   [=, i = size_t(0)](auto) mutable { return i++ != sub_idx; });
    }
    std::erase(m.drawables, &draw_context_menu);

    return m;
  });
}

void addArc(model::Model::NodeType source_node_type, size_t source,
            size_t target, symmetri::Token color) {
  rxdispatch::push([=](model::Model&& m) mutable {
    const size_t transition_idx =
        model::Model::NodeType::Transition == source_node_type ? source
                                                               : target;
    // remove transition from view
    m.t_view.erase(
        std::remove(m.t_view.begin(), m.t_view.end(), transition_idx),
        m.t_view.end());
    // add it again to the view...
    const size_t new_transition_idx = m.net.transition.size();
    m.net.transition.push_back(m.net.transition[transition_idx]);
    m.net.output_n.push_back(m.net.output_n[transition_idx]);
    m.net.input_n.push_back(m.net.input_n[transition_idx]);
    m.net.priority.push_back(m.net.priority[transition_idx]);
    m.net.store.emplace_back(symmetri::identity<symmetri::DirectMutation>{});

    m.t_positions.push_back(m.t_positions[transition_idx]);
    m.t_view.push_back(new_transition_idx);

    // add the arc
    auto& p_to_ts = m.net.p_to_ts_n[source];
    switch (source_node_type) {
      case model::Model::NodeType::Place:
        m.net.input_n[new_transition_idx].push_back({source, color});
        if (std::find(p_to_ts.begin(), p_to_ts.end(), transition_idx) ==
            p_to_ts.end()) {
          p_to_ts.push_back(new_transition_idx);
        }
        break;
      case model::Model::NodeType::Transition:
        m.net.output_n[new_transition_idx].push_back({target, color});
        break;
    };

    std::erase(m.drawables, &draw_context_menu);
    return m;
  });
}

void removePlace(size_t idx) {
  rxdispatch::push([=](model::Model&& m) mutable {
    // remove node from view
    m.p_view.erase(std::remove(m.p_view.begin(), m.p_view.end(), idx),
                   m.p_view.end());
    // remove selection of this node
    std::erase(m.p_highlight, idx);
    // remove marking for this node
    auto [b, e] = std::ranges::remove_if(
        m.tokens, [idx](const symmetri::AugmentedToken at) {
          return std::get<size_t>(at) == idx;
        });
    m.tokens.erase(b, e);
    std::erase(m.drawables, &draw_context_menu);
    return m;
  });
}

void removeTransition(size_t idx) {
  rxdispatch::push([=](model::Model&& m) mutable {
    m.t_view.erase(std::remove(m.t_view.begin(), m.t_view.end(), idx),
                   m.t_view.end());
    std::erase(m.t_highlight, idx);
    std::erase(m.drawables, &draw_context_menu);
    return m;
  });
}

void updateArcColor(model::Model::NodeType source_node_type, size_t idx,
                    size_t sub_idx, const symmetri::Token color) {
  rxdispatch::push([=](model::Model&& m) mutable {
    std::get<symmetri::Token>((source_node_type == model::Model::NodeType::Place
                                   ? m.net.input_n
                                   : m.net.output_n)[idx][sub_idx]) = color;
    return m;
  });
}

int updatePriority(ImGuiInputTextCallbackData* data) {
  size_t idx = *static_cast<size_t*>(data->UserData);
  data->ClearSelection();
  if ((data->Buf != NULL) && (data->Buf[0] == '\0')) {
  } else if (strcmp(data->Buf, "-") == 0) {
  } else {
    const auto priority = std::clamp(std::stoi(data->Buf), INT8_MIN, INT8_MAX);
    rxdispatch::push([=](model::Model&& m) mutable {
      m.net.priority[idx] = priority;
      return m;
    });
  }
  return 0;
}

int updatePlaceName(ImGuiInputTextCallbackData* data) {
  size_t idx = *static_cast<size_t*>(data->UserData);
  data->ClearSelection();
  rxdispatch::push(
      [=, name = std::string(data->Buf)](model::Model&& m) mutable {
        m.net.place[idx] = name;
        return m;
      });
  return 0;
}

int updateTransitionName(ImGuiInputTextCallbackData* data) {
  // we add 1000 to distinguish from places.. hsould be neater someday.
  size_t idx = (*static_cast<size_t*>(data->UserData)) - 1000;
  data->ClearSelection();

  rxdispatch::push(
      [=, name = std::string(data->Buf)](model::Model&& m) mutable {
        m.net.transition[idx] = name;
        return m;
      });
  return 0;
}

void setContextMenuActive(ImVec2 world_pos) {
  rxdispatch::push([=](model::Model&& m) {
    m.drawables.push_back(&draw_context_menu);
    m.context_menu_pos = {world_pos.x, world_pos.y};
    return m;
  });
}

void setContextMenuInactive() {
  rxdispatch::push([](model::Model&& m) {
    std::erase(m.drawables, &draw_context_menu);
    m.context_menu_pos.reset();
    return m;
  });
}

void setSelectedNode(model::Model::NodeType node_type, size_t idx) {
  rxdispatch::push([=](model::Model&& m) {
    m.selected_node_idx = {node_type, idx};
    m.p_highlight.clear();
    m.t_highlight.clear();
    m.arc_highlight.clear();
    m.selected_arc_idxs.reset();
    (node_type == model::Model::NodeType::Place ? m.p_highlight : m.t_highlight)
        .push_back(idx);
    return m;
  });
};

void addHighlightNode(model::Model::NodeType node_type, size_t idx) {
  rxdispatch::push([=](model::Model&& m) {
    (node_type == model::Model::NodeType::Place ? m.p_highlight : m.t_highlight)
        .push_back(idx);
    return m;
  });
};

void setSelectedTargetNode(model::Model::NodeType node_type, size_t idx) {
  rxdispatch::push([=](model::Model&& m) {
    auto& c = (node_type == model::Model::NodeType::Place ? m.p_highlight
                                                          : m.t_highlight);
    c.clear();
    c.push_back(idx);
    return m;
  });
};

void resetSelection() {
  rxdispatch::push([](model::Model&& m) {
    m.selected_node_idx.reset();
    m.p_highlight.clear();
    m.t_highlight.clear();
    m.selected_arc_idxs.reset();
    m.arc_highlight.clear();
    return m;
  });
};

void addHighlightArc(model::Model::NodeType source_node_type, size_t t_idx,
                     size_t sub_idx) {
  rxdispatch::push([=](model::Model&& m) {
    m.arc_highlight.emplace_back(source_node_type, t_idx, sub_idx);
    return m;
  });
};

void setSelectedArc(model::Model::NodeType source_node_type, size_t t_idx,
                    size_t sub_idx) {
  rxdispatch::push([=](model::Model&& m) {
    m.selected_node_idx.reset();
    m.p_highlight.clear();
    m.t_highlight.clear();
    m.arc_highlight.clear();
    m.selected_arc_idxs = {source_node_type, t_idx, sub_idx};
    m.arc_highlight.emplace_back(source_node_type, t_idx, sub_idx);
    return m;
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
    rxdispatch::push([file_name = std::string(data->Buf)](model::Model&& m) {
      m.active_file = std::filesystem::path(file_name);
      return m;
    });
  }
  return 0;
}

void updateColorTable() {
  rxdispatch::push([](model::Model&& m) {
    m.colors = symmetri::Token::getColors();
    return m;
  });
}

void addTokenToPlace(symmetri::AugmentedToken token) {
  rxdispatch::push([=](model::Model&& m) {
    m.tokens.push_back(token);
    std::erase(m.drawables, &draw_context_menu);
    return m;
  });
}

void removeTokenFromPlace(symmetri::AugmentedToken token) {
  rxdispatch::push([=](model::Model&& m) {
    if (auto it = std::ranges::find(m.tokens, token); it != m.tokens.end()) {
      m.tokens.erase(it);
    }
    return m;
  });
}

void resetSelectedTargetNode() {
  rxdispatch::push([](model::Model&& m) {
    m.selected_arc_idxs.reset();
    return m;
  });
}

void zoomRelative(float f) {
  rxdispatch::push([=](model::Model&& model) {
    model.zoom_factor += f;
    return model;
  });
}

void zoomAbsolute(float f) {
  rxdispatch::push([=](model::Model&& model) {
    model.zoom_factor = f;
    return model;
  });
}

void resetNetView() {
  rxdispatch::push([=](model::Model&& model) {
    auto& m = model;
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

    float min_x = std::min(getMinX(m.t_view, m.t_positions),
                           getMinX(m.p_view, m.p_positions));
    float min_y = std::min(getMinY(m.t_view, m.t_positions),
                           getMinY(m.p_view, m.p_positions));

    m.scrolling = {400 - min_x, 120 - min_y};

    return model;
  });
}

void tryFire(size_t transition_idx) {
  rxdispatch::push([=](model::Model&& m) {
    if (m.net.input_n[transition_idx].empty() ||
        canFire(m.net.input_n[transition_idx], m.tokens)) {
      // deduct
      deductMarking(m.tokens, m.net.input_n[transition_idx]);
      // add
      const auto& lookup_t = m.net.output_n[transition_idx];
      const auto result = fire(m.net.store[transition_idx]);
      for (const auto& [p, c] : lookup_t) {
        m.tokens.emplace_back(p, result);
      }
      m.log.push_back({transition_idx, result, symmetri::Clock::now()});
    }
    return m;
  });
}
void updateTransitionOutputColor(size_t transition_idx, symmetri::Token color) {
  rxdispatch::push([=](model::Model&& m) {
    m.net.store[transition_idx] = [=] { return color; };
    return m;
  });
}
