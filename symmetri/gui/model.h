#pragma once
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

#include "graph.hpp"
#include "imgui.h"
#include "menu_bar.h"
#include "symmetri/parsers.h"

namespace model {

struct Model {
  struct shared {
    std::chrono::steady_clock::time_point timestamp;
    std::filesystem::path working_dir;
    std::optional<std::filesystem::path> active_file;
    int menu_height = 20;
    std::vector<Arc> arcs = {};
    std::vector<Node> nodes = {};
    std::vector<size_t> a_idx = {};
    std::vector<size_t> n_idx = {};
    Graph graph;
    ImGui::FileBrowser file_dialog;
  };
  std::shared_ptr<shared> data = std::make_shared<shared>();
};

struct ViewModel {
  ViewModel() {}
  explicit ViewModel(const Model& m) : m(m) { auto& model = *m.data; }
  Model m;
  struct shared {};
  std::shared_ptr<shared> data = std::make_shared<shared>();
};

using Reducer = std::function<Model(Model&)>;
inline auto noop = Reducer([](Model& m) { return std::move(m); });

inline Model initializeModel() {
  Model m_ptr;
  auto& m = *m_ptr.data;
  m.working_dir = "/users/thomashorstink/Projects/symmetri/nets";
  m.active_file =
      "/Users/thomashorstink/Projects/Symmetri/examples/combinations/"
      "DualProcessWorker.pnml";
  symmetri::Net net;
  symmetri::Marking marking;
  symmetri::PriorityTable pt;
  const std::filesystem::path pn_file = m.active_file.value();
  if (pn_file.extension() == std::string(".pnml")) {
    std::tie(net, marking) = symmetri::readPnml({pn_file});
  } else {
    std::tie(net, marking, pt) = symmetri::readGrml({pn_file});
  }
  auto g = createGraph(net);
  m.graph = *g;
  m.arcs = g->arcs;
  m.nodes = g->nodes;
  m.a_idx = g->a_idx;
  m.n_idx = g->n_idx;

  m.file_dialog.SetTitle("title");
  m.file_dialog.SetTypeFilters({".pnml", ".grml"});
  m.file_dialog.SetPwd(m.working_dir);
  return m_ptr;
}

inline void draw(const ViewModel& vm) {
  auto& m = *vm.m.data;
  draw_menu_bar(m.file_dialog);

  draw(m.graph);
  ImGui::End();
}

}  // namespace model
