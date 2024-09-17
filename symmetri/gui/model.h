#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>

#include "petri.h"

namespace model {

struct ViewModel;

using Drawable = void (*)(const ViewModel &);

struct Coordinate {
  float x, y;
};

Coordinate operator+(Coordinate &lhs, Coordinate &rhs);
Coordinate &operator+=(Coordinate &lhs, const Coordinate &rhs);

struct Model {
  struct shared {
    bool show_grid, context_menu_active;
    Coordinate scrolling;
    std::optional<std::tuple<bool, size_t, size_t>> selected_arc_idxs;
    std::optional<std::tuple<bool, size_t>> selected_node_idx,
        selected_target_node_idx;

    std::chrono::steady_clock::time_point timestamp;
    std::filesystem::path working_dir;
    std::optional<std::filesystem::path> active_file;
    std::vector<Coordinate> t_positions, p_positions;
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
  Coordinate scrolling;
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
  const std::vector<Coordinate> &t_positions, p_positions;
  std::vector<size_t> t_fireable;

  ViewModel() = delete;
  ViewModel(Model m);
};

using Reducer = std::function<Model(Model &&)>;

Model initializeModel();

}  // namespace model
