#include "model.h"

#include <mutex>
#include <numeric>

#include "load_file.h"
namespace model {

Model initializeModel() {
  Model m_ptr;
  auto& m = *m_ptr.data;
  m.show_grid = true;
  m.context_menu_active = false;
  m.selected_arc = nullptr;
  m.selected_node = nullptr;

  m.scrolling = ImVec2(0.0f, 0.0f);

  m.working_dir = "/users/thomashorstink/Projects/symmetri/nets";
  std::filesystem::path p =
      "/Users/thomashorstink/Projects/Symmetri/examples/combinations/"
      "DualProcessWorker.pnml";
  loadPetriNet(p);

  return m_ptr;
}

ViewModel::ViewModel(const Model& m)
    : show_grid(m.data->show_grid),
      context_menu_active(m.data->context_menu_active),
      scrolling(m.data->scrolling),
      selected_arc(m.data->selected_arc),
      selected_node(m.data->selected_node),
      t_view(m.data->t_view),
      p_view(m.data->p_view),
      colors(m.data->colors),
      net(m.data->net),
      t_positions(m.data->t_positions),
      p_positions(m.data->p_positions) {
  static std::once_flag flag;
  std::call_once(flag, [&] {
    file_dialog.SetTitle("title");
    file_dialog.SetTypeFilters({".pnml", ".grml"});
    file_dialog.SetPwd(m.data->working_dir);
  });
}

}  // namespace model
