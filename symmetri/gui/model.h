#pragma once

#include <stddef.h>

#include <filesystem>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "petri.h"
#include "symmetri/colors.hpp"

namespace model {

struct ViewModel;

using Drawable = void (*)(const ViewModel&);

struct Coordinate {
  float x, y;
};
Coordinate operator*(float lhs, Coordinate rhs);
Coordinate operator+(Coordinate& lhs, Coordinate& rhs);
Coordinate& operator+=(Coordinate& lhs, const Coordinate& rhs);

struct Model {
  // node-type
  enum class NodeType { Place, Transition };
  // (source-node-type, transition idx, place idx)
  using Arc = std::tuple<NodeType, size_t, size_t>;
  // (node-type, node idx)
  using Node = std::tuple<NodeType, size_t>;

  struct shared {
    shared();
    bool show_grid = true;
    Coordinate scrolling;
    std::optional<Coordinate> context_menu_pos;
    std::optional<Arc> selected_arc_idxs;
    std::optional<Node> selected_node_idx;
    std::optional<std::filesystem::path> active_file;
    float zoom_factor = 1.0f;
    std::map<Drawable, std::promise<void>> blockers;
    std::vector<Coordinate> t_positions, p_positions;
    std::vector<size_t> t_view, p_view;
    std::vector<size_t> t_highlight, p_highlight;
    std::vector<Arc> arc_highlight;
    bool arc_hovered = false;
    std::vector<std::string_view> colors = symmetri::Token::getColors();
    std::vector<symmetri::AugmentedToken> tokens;
    symmetri::SmallLog log;
    std::vector<Drawable> drawables;
    symmetri::Petri::PTNet net;
  };
  Model() : data(std::make_shared<shared>()) {}
  std::shared_ptr<shared> data;
};

struct ViewModel {
  const std::vector<Drawable> drawables;
  const float kMenuBarHeight = 20;
  const float kMenuWidth = 350;
  const bool show_grid;
  const Coordinate scrolling;
  const std::optional<Coordinate> context_menu_pos;
  // source NodeType, source index, target sub-index
  std::optional<std::tuple<model::Model::NodeType, size_t, size_t>>
      selected_arc_idxs;
  // NodeType | index
  std::optional<std::tuple<model::Model::NodeType, size_t>> selected_node_idx;
  const std::string active_file;
  const float zoom_factor;
  const std::vector<size_t> t_view, p_view;
  const std::vector<size_t> t_highlight, p_highlight;
  const std::vector<Model::Arc> arc_highlight;
  const Coordinate node_size;
  const std::vector<std::string_view> colors;
  const std::vector<symmetri::AugmentedToken> tokens;
  const symmetri::SmallLog& log;

  const symmetri::Petri::PTNet& net;
  const std::vector<Coordinate>&t_positions, &p_positions;
  const std::vector<size_t> t_fireable;
  const bool arc_hovered;

  ViewModel() = delete;
  ViewModel(Model m);
};

using Reducer = std::function<Model(Model&&)>;
using Computer = std::function<Reducer(void)>;

Model initializeModel();

}  // namespace model
