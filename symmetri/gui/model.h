#pragma once
// clang-format off
#include "imgui.h"
// clang-format on

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

#include "graph.hpp"
#include "imfilebrowser.h"
#include "position_parsers.h"
#include "symmetri/parsers.h"

namespace model {

struct Model {
  struct shared {
    bool show_grid;
    ImVec2 scrolling;

    std::chrono::steady_clock::time_point timestamp;
    std::filesystem::path working_dir;
    std::optional<std::filesystem::path> active_file;
    int menu_height = 20;
    Graph graph;
    ImGui::FileBrowser file_dialog;
  };
  std::shared_ptr<shared> data = std::make_shared<shared>();
};

struct ViewModel {
  ViewModel() {}
  explicit ViewModel(const Model& m) : m(m) {
    auto& model = *m.data;
    data->a_idx = model.graph.a_idx;
    data->n_idx = model.graph.n_idx;
  }
  Model m;
  struct shared {
    std::vector<size_t> a_idx = {};
    std::vector<size_t> n_idx = {};
  };
  std::shared_ptr<shared> data = std::make_shared<shared>();
};

using Reducer = std::function<Model(Model&&)>;
inline auto noop = Reducer([](Model&& m) { return std::move(m); });

inline Model initializeModel() {
  Model m_ptr;
  auto& m = *m_ptr.data;
  m.show_grid = true;
  m.scrolling = ImVec2(0.0f, 0.0f);
  m.working_dir = "/users/thomashorstink/Projects/symmetri/nets";
  m.active_file =
      "/Users/thomashorstink/Projects/Symmetri/examples/combinations/"
      "DualProcessWorker.pnml";
  symmetri::Net net;
  symmetri::Marking marking;
  symmetri::PriorityTable pt;
  std::map<std::string, std::pair<float, float>> positions;
  const std::filesystem::path pn_file = m.active_file.value();
  if (pn_file.extension() == std::string(".pnml")) {
    std::tie(net, marking) = symmetri::readPnml({pn_file});
    positions = farbart::readPnmlPositions({pn_file});
  } else {
    std::tie(net, marking, pt) = symmetri::readGrml({pn_file});
  }
  m.graph = *createGraph(net, positions);

  m.file_dialog.SetTitle("title");
  m.file_dialog.SetTypeFilters({".pnml", ".grml"});
  m.file_dialog.SetPwd(m.working_dir);
  return m_ptr;
}

}  // namespace model
