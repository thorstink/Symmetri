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
#include "petri.h"
namespace model {

struct Model {
  // Model() {};
  // Model(const Model&) = delete;
  // Model& operator=(const Model&) = delete;
  struct shared {
    bool show_grid, context_menu_active;
    ImVec2 scrolling;
    const Node *node_selected, *node_hovered_in_list, *node_hovered_in_scene;
    const Arc *arc_selected, *arc_hovered_in_scene;
    std::chrono::steady_clock::time_point timestamp;
    std::filesystem::path working_dir;
    std::optional<std::filesystem::path> active_file;
    Graph graph;
    std::shared_ptr<symmetri::Petri> petri;
    ImGui::FileBrowser file_dialog;
  };
  std::shared_ptr<shared> data = std::make_shared<shared>();
};

struct ViewModel {
  bool show_grid, context_menu_active;
  ImVec2 scrolling;
  const Node *node_selected, *node_hovered_in_list, *node_hovered_in_scene;
  const Arc *arc_selected, *arc_hovered_in_scene;
  std::vector<size_t> a_idx;
  std::vector<size_t> n_idx;
  Graph graph;
  ImGui::FileBrowser *file_dialog;
  ViewModel() = delete;
  explicit ViewModel(const Model &m)
      : m(m),
        show_grid(m.data->show_grid),
        context_menu_active(m.data->context_menu_active),
        scrolling(m.data->scrolling),
        node_selected(m.data->node_selected),
        node_hovered_in_list(m.data->node_hovered_in_list),
        node_hovered_in_scene(m.data->node_hovered_in_scene),
        arc_selected(m.data->arc_selected),
        arc_hovered_in_scene(m.data->arc_hovered_in_scene),
        a_idx(m.data->graph.a_idx),
        n_idx(m.data->graph.n_idx),
        graph(m.data->graph),
        file_dialog(&m.data->file_dialog) {
    auto &model = *m.data;
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

using Reducer = std::function<Model(Model &&)>;

Model initializeModel();

}  // namespace model
