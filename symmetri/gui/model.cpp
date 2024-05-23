#include "model.h"

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

  m.file_dialog.SetTitle("title");
  m.file_dialog.SetTypeFilters({".pnml", ".grml"});
  m.file_dialog.SetPwd(m.working_dir);
  return m_ptr;
}
}  // namespace model
