#include "model.h"

#include <algorithm>
#include <numeric>
#include <utility>

#include "draw_graph.h"
#include "draw_menu_bar.h"
#include "draw_tools_menu.h"
#include "petri.h"

namespace model {

Coordinate operator+(Coordinate& lhs, Coordinate& rhs) {
  return {lhs.x + rhs.x, lhs.y + rhs.y};
}
Coordinate& operator+=(Coordinate& lhs, const Coordinate& rhs) {
  lhs.x += rhs.x;
  lhs.y += rhs.y;
  return lhs;
}

Coordinate operator*(float lhs, Coordinate rhs) {
  rhs.x *= lhs;
  rhs.y *= lhs;
  return rhs;
}

Model::shared::shared()
    : drawables({&draw_menu_bar, &draw_graph, &draw_tools_menu}) {}

ViewModel::ViewModel(Model m)
    : drawables(m.data->drawables),
      show_grid(m.data->show_grid),
      scrolling(m.data->scrolling),
      context_menu_pos(m.data->context_menu_pos),
      selected_arc_idxs(m.data->selected_arc_idxs),
      selected_node_idx(m.data->selected_node_idx),
      active_file(
          m.data->active_file.value_or(std::filesystem::current_path())),
      zoom_factor(m.data->zoom_factor),
      t_view(m.data->t_view),
      p_view(m.data->p_view),
      t_highlight(m.data->t_highlight),
      p_highlight(m.data->p_highlight),
      arc_highlight(m.data->arc_highlight),
      node_size(32.f * zoom_factor, 32.f * zoom_factor),
      colors(m.data->colors),
      tokens(m.data->tokens),
      net(m.data->net),
      t_positions(m.data->t_positions),
      p_positions(m.data->p_positions),
      t_fireable(std::accumulate(
          t_view.begin(), t_view.end(), std::vector<size_t>{},
          [this](std::vector<size_t>&& t_fireable, size_t t_idx) {
            if (canFire(net.input_n.at(t_idx), tokens) ||
                net.input_n.at(t_idx).empty()) {
              t_fireable.push_back(t_idx);
            }
            std::sort(t_fireable.begin(), t_fireable.end(),
                      [&](const auto& a, const auto& b) {
                        return net.priority[a] > net.priority[b] ||
                               net.input_n.at(a).size() >
                                   net.input_n.at(b).size();
                      });

            return std::move(t_fireable);
          })),
      arc_hovered(m.data->arc_hovered) {}

}  // namespace model
