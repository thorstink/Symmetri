#pragma once
// clang-format off
#include "imgui.h"
// clang-format on

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>

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
    const symmetri::AugmentedToken *selected_arc;
    const std::string *selected_node;

    const Node *node_selected, *node_hovered_in_list, *node_hovered_in_scene;
    const Arc *arc_selected, *arc_hovered_in_scene;
    std::chrono::steady_clock::time_point timestamp;
    std::filesystem::path working_dir;
    std::optional<std::filesystem::path> active_file;
    Graph graph;
    std::vector<ImVec2> t_positions, p_positions;
    std::vector<size_t> t_view, p_view;
    symmetri::Petri::PTNet net;
    ImGui::FileBrowser file_dialog;
  };
  std::shared_ptr<shared> data = std::make_shared<shared>();
};

struct ViewModel {
  bool show_grid, context_menu_active;
  ImVec2 scrolling;
  const symmetri::AugmentedToken *selected_arc;
  const std::string *selected_node;

  const Node *node_selected, *node_hovered_in_list, *node_hovered_in_scene;
  const Arc *arc_selected, *arc_hovered_in_scene;
  std::vector<size_t> a_idx;
  std::vector<size_t> n_idx;
  std::vector<size_t> t_view, p_view;
  const symmetri::Petri::PTNet &net;
  const std::vector<ImVec2> &t_positions, p_positions;

  const Graph &graph;
  ImGui::FileBrowser *file_dialog;
  ViewModel() = delete;
  explicit ViewModel(const Model &m)
      : show_grid(m.data->show_grid),
        context_menu_active(m.data->context_menu_active),
        scrolling(m.data->scrolling),
        selected_arc(m.data->selected_arc),
        selected_node(m.data->selected_node),
        node_selected(m.data->node_selected),
        node_hovered_in_list(m.data->node_hovered_in_list),
        node_hovered_in_scene(m.data->node_hovered_in_scene),
        arc_selected(m.data->arc_selected),
        arc_hovered_in_scene(m.data->arc_hovered_in_scene),
        t_view(m.data->t_view),
        p_view(m.data->p_view),
        net(m.data->net),
        t_positions(m.data->t_positions),
        p_positions(m.data->p_positions),
        a_idx(m.data->graph.a_idx),
        n_idx(m.data->graph.n_idx),
        graph(m.data->graph),
        file_dialog(&m.data->file_dialog) {}
};

using Reducer = std::function<Model(Model &&)>;

Model initializeModel();

}  // namespace model
