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

namespace model {

struct Model {
  struct shared {
    bool show_grid, context_menu_active;
    ImVec2 scrolling;
    const Node *node_selected, *node_hovered_in_list, *node_hovered_in_scene;
    const Arc *arc_selected, *arc_hovered_in_scene;
    std::chrono::steady_clock::time_point timestamp;
    std::filesystem::path working_dir;
    std::optional<std::filesystem::path> active_file;
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

Model initializeModel();

}  // namespace model
