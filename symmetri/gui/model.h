#pragma once

// clang-format off
#include "imgui.h"
#include "imgui_internal.h"

// clang-format on

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>

#include "imfilebrowser.h"
#include "petri.h"

namespace model {

struct ViewModel;

using Drawable = void (*)(const ViewModel &);

struct Model {
  struct shared {
    bool show_grid, context_menu_active;
    ImVec2 scrolling;
    std::optional<std::tuple<bool, size_t, size_t>> selected_arc_idxs;
    std::optional<std::tuple<bool, size_t>> selected_node_idx,
        selected_target_node_idx;

    std::chrono::steady_clock::time_point timestamp;
    std::filesystem::path working_dir;
    std::optional<std::filesystem::path> active_file;
    std::vector<ImVec2> t_positions, p_positions;
    std::vector<size_t> t_view, p_view;
    std::vector<const char *> colors = symmetri::Token::getColors();
    std::vector<symmetri::AugmentedToken> tokens;
    std::vector<Drawable> drawables;
    symmetri::Petri::PTNet net;
  };
  std::shared_ptr<shared> data = std::make_shared<shared>();
};

struct ViewModel {
  std::vector<Drawable> drawables;
  bool show_grid, context_menu_active;
  ImVec2 scrolling;
  // is place, index, sub-index
  std::optional<std::tuple<bool, size_t, size_t>> selected_arc_idxs;
  // is place | index
  std::optional<std::tuple<bool, size_t>> selected_node_idx,
      selected_target_node_idx;
  const std::string active_file;

  std::vector<size_t> t_view, p_view;
  std::vector<const char *> colors;
  std::vector<symmetri::AugmentedToken> tokens;

  const symmetri::Petri::PTNet &net;
  const std::vector<ImVec2> &t_positions, p_positions;
  std::vector<size_t> t_fireable;

  static inline ImGui::FileBrowser file_dialog = ImGui::FileBrowser();
  ViewModel() = delete;
  ViewModel(Model m);
};

using Reducer = std::function<Model(Model &&)>;

Model initializeModel();

}  // namespace model
