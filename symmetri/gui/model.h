#pragma once

#include <filesystem>
#include <future>
#include <map>
#include <memory>
#include <optional>

#include "petri.h"

namespace model {

struct ViewModel;

using Drawable = void (*)(const ViewModel&);

struct Coordinate {
  float x, y;
};

Coordinate operator+(Coordinate& lhs, Coordinate& rhs);
Coordinate& operator+=(Coordinate& lhs, const Coordinate& rhs);

struct Model {
  enum class NodeType { Place, Transition };
  // using SelectedNode = std::pair<NodeType, size_t>;

  struct shared {
    bool show_grid = true;
    Coordinate scrolling;
    std::optional<std::tuple<NodeType, size_t, size_t>> selected_arc_idxs;
    std::optional<std::tuple<NodeType, size_t>> selected_node_idx,
        selected_target_node_idx;

    std::optional<std::filesystem::path> active_file;
    std::map<Drawable, std::promise<void>> blockers;
    std::vector<Coordinate> t_positions, p_positions;
    std::vector<size_t> t_view, p_view;
    std::vector<std::string_view> colors = symmetri::Token::getColors();
    std::vector<symmetri::AugmentedToken> tokens;
    std::vector<Drawable> drawables;
    symmetri::Petri::PTNet net;
  };
  Model() : data(std::make_shared<shared>()) {}
  std::shared_ptr<shared> data;
};

struct ViewModel {
  const std::vector<Drawable> drawables;
  const bool show_grid;
  const Coordinate scrolling;
  // source NodeType, source index, target sub-index
  std::optional<std::tuple<model::Model::NodeType, size_t, size_t>>
      selected_arc_idxs;
  // NodeType | index
  std::optional<std::tuple<model::Model::NodeType, size_t>> selected_node_idx,
      selected_target_node_idx;
  const std::string active_file;

  const std::vector<size_t> t_view, p_view;
  const std::vector<std::string_view> colors;
  const std::vector<symmetri::AugmentedToken> tokens;

  const symmetri::Petri::PTNet& net;
  const std::vector<Coordinate>&t_positions, &p_positions;
  const std::vector<size_t> t_fireable;

  ViewModel() = delete;
  ViewModel(Model m);
};

using Reducer = std::function<Model(Model&&)>;
using Computer = std::function<Reducer(void)>;

Model initializeModel();

}  // namespace model
