#include "model.h"

#include <mutex>
#include <numeric>
#include <ranges>

#include "draw_ui.h"
#include "load_file.h"
#include "menu_bar.h"
#include "petri.h"
namespace model {

Coordinate operator+(Coordinate& lhs, Coordinate& rhs) {
  return {lhs.x + rhs.x, lhs.y + rhs.y};
}
Coordinate operator+=(Coordinate lhs, const Coordinate& rhs) {
  lhs.x = rhs.x;
  lhs.y = rhs.y;
  return lhs;
}

Model initializeModel() {
  Model model;
  auto& m = *model.data;
  m.show_grid = true;
  m.context_menu_active = false;
  m.scrolling = Coordinate({0.0f, 0.0f});

  m.working_dir = "/users/thomashorstink/Projects/symmetri/nets";
  std::filesystem::path p =
      "/users/thomashorstink/Projects/symmetri/nets/n1.pnml";
  loadPetriNet(p);

  return model;
}

ViewModel::ViewModel(Model m)
    : drawables([&] {
        std::vector<Drawable> drawables;
        drawables.push_back(&draw_menu_bar);
        drawables.push_back(&draw_interface);

        for (auto&& drawable : m.data->drawables) {
          drawables.push_back(drawable);
        }
        return drawables;
      }()),
      show_grid(m.data->show_grid),
      context_menu_active(m.data->context_menu_active),
      scrolling(m.data->scrolling),
      selected_arc_idxs(m.data->selected_arc_idxs),
      selected_node_idx(m.data->selected_node_idx),
      selected_target_node_idx(m.data->selected_target_node_idx),
      active_file(m.data->active_file.value_or("No file")),
      t_view(m.data->t_view),
      p_view(m.data->p_view),
      colors(m.data->colors),
      tokens(m.data->tokens),
      net(m.data->net),
      t_positions(m.data->t_positions),
      p_positions(m.data->p_positions),
      t_fireable(std::accumulate(
          t_view.begin(), t_view.end(), std::vector<size_t>{},
          [this](std::vector<size_t>&& t_fireable, size_t t_idx) {
            if (canFire(net.input_n[t_idx], tokens)) {
              t_fireable.push_back(t_idx);
            }
            return std::move(t_fireable);
          })) {}

}  // namespace model
