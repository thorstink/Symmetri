#define IMGUI_DEFINE_MATH_OPERATORS
#include "graph_reducers.h"

#include "imgui_internal.h"
#include "rxdispatch.h"

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

void removeArc(bool is_input, size_t transition_idx, size_t sub_idx) {
  rxdispatch::push([=](model::Model&& m) mutable {
    // remove transition from view
    m.data->t_view.erase(std::remove(m.data->t_view.begin(),
                                     m.data->t_view.end(), transition_idx),
                         m.data->t_view.end());

    const size_t new_idx = m.data->net.transition.size();
    // add it again to the view...
    m.data->net.transition.push_back(m.data->net.transition[transition_idx]);
    m.data->net.priority.push_back(m.data->net.priority[transition_idx]);
    m.data->t_positions.push_back(m.data->t_positions[transition_idx]);
    m.data->t_view.push_back(new_idx);

    // now copy the arcs, except for the one we want to delete
    if (is_input) {
      m.data->net.output_n.push_back(m.data->net.output_n[transition_idx]);
      m.data->net.input_n.push_back({});
      std::copy_if(
          m.data->net.input_n[transition_idx].begin(),
          m.data->net.input_n[transition_idx].end(),
          std::back_inserter(m.data->net.input_n.back()),
          [i = size_t(0), sub_idx](auto) mutable { return i++ != sub_idx; });

    } else {
      m.data->net.input_n.push_back(m.data->net.input_n[transition_idx]);
      m.data->net.output_n.push_back({});
      std::copy_if(
          m.data->net.output_n[transition_idx].begin(),
          m.data->net.output_n[transition_idx].end(),
          std::back_inserter(m.data->net.output_n.back()),
          [i = size_t(0), sub_idx](auto) mutable { return i++ != sub_idx; });
    }

    return m;
  });
}

void addArc(bool is_place, size_t source, size_t target,
            symmetri::Token color) {
  rxdispatch::push([=](model::Model&& m) mutable {
    const size_t transition_idx = is_place ? target : source;
    // remove transition from view
    m.data->t_view.erase(std::remove(m.data->t_view.begin(),
                                     m.data->t_view.end(), transition_idx),
                         m.data->t_view.end());

    const size_t new_idx = m.data->net.transition.size();
    // add it again to the view...
    m.data->net.transition.push_back(m.data->net.transition[transition_idx]);
    m.data->net.output_n.push_back(m.data->net.output_n[transition_idx]);
    m.data->net.input_n.push_back(m.data->net.input_n[transition_idx]);
    m.data->net.priority.push_back(m.data->net.priority[transition_idx]);
    m.data->t_positions.push_back(m.data->t_positions[transition_idx]);
    m.data->t_view.push_back(new_idx);

    // add the arc
    is_place ? m.data->net.input_n[new_idx].push_back({source, color})
             : m.data->net.output_n[new_idx].push_back({target, color});

    return m;
  });
}

void removePlace(size_t idx) {
  rxdispatch::push([=](model::Model&& m) mutable {
    m.data->p_view.erase(
        std::remove(m.data->p_view.begin(), m.data->p_view.end(), idx),
        m.data->p_view.end());
    m.data->selected_node_idx.reset();
    return m;
  });
}

void removeTransition(size_t idx) {
  rxdispatch::push([=](model::Model&& m) mutable {
    m.data->t_view.erase(
        std::remove(m.data->t_view.begin(), m.data->t_view.end(), idx),
        m.data->t_view.end());
    m.data->selected_node_idx.reset();
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
void updateArcColor(bool is_input, size_t idx, size_t sub_idx,
                    const symmetri::Token color) {
  rxdispatch::push([=](model::Model&& m) mutable {
    (is_input ? m.data->net.input_n : m.data->net.output_n)[idx][sub_idx]
        .color = color;
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

void setSelectedNode(bool is_place, size_t idx) {
  rxdispatch::push([=](model::Model&& m) {
    m.data->selected_node_idx = {is_place, idx};
    m.data->selected_arc_idxs.reset();
    return m;
  });
};

void resetSelection() {
  rxdispatch::push([](model::Model&& m) {
    m.data->selected_node_idx.reset();
    m.data->selected_arc_idxs.reset();
    return m;
  });
};

void setSelectedArc(bool is_input, size_t idx, size_t sub_idx) {
  rxdispatch::push([=](model::Model&& m) {
    m.data->selected_node_idx.reset();
    m.data->selected_arc_idxs = {is_input, idx, sub_idx};
    return m;
  });
};

void renderNodeEntry(bool is_place, const std::string& name, size_t idx,
                     bool selected) {
  ImGui::PushID(idx);
  if (ImGui::Selectable(name.c_str(), selected)) {
    setSelectedNode(is_place, idx);
  }
  ImGui::PopID();
}
