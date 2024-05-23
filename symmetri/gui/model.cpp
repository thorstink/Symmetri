#include "model.h"

#include <mutex>
#include <numeric>

#include "position_parsers.h"
#include "symmetri/parsers.h"
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
  m.active_file =
      "/Users/thomashorstink/Projects/Symmetri/examples/combinations/"
      "DualProcessWorker.pnml";
  symmetri::Net net;
  symmetri::Marking marking;
  symmetri::PriorityTable pt;
  std::map<std::string, ImVec2> positions;

  const std::filesystem::path pn_file = m.active_file.value();
  if (pn_file.extension() == std::string(".pnml")) {
    std::tie(net, marking) = symmetri::readPnml({pn_file});
    positions = farbart::readPnmlPositions({pn_file});
  } else {
    std::tie(net, marking, pt) = symmetri::readGrml({pn_file});
  }

  m.net = symmetri::Petri(net, pt, marking, symmetri::Marking{}, "active",
                          std::make_shared<symmetri::TaskSystem>(1))
              .net;
  for (const auto& transition : m.net.transition) {
    m.t_positions.push_back(positions.at(transition));
  }
  for (const auto& place : m.net.place) {
    m.p_positions.push_back(positions.at(place));
  }
  m.t_view.resize(m.net.transition.size());
  m.p_view.resize(m.net.place.size());
  std::iota(m.t_view.begin(), m.t_view.end(), 0);
  std::iota(m.p_view.begin(), m.p_view.end(), 0);

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
      net(m.data->net),
      t_positions(m.data->t_positions),
      p_positions(m.data->p_positions) {
  static std::once_flag flag;
  std::call_once(flag, [&] {
    file_dialog.SetTitle("title");
    file_dialog.SetTypeFilters({".pnml", ".grml"});
  });
  file_dialog.SetPwd(m.data->working_dir);
}

}  // namespace model
