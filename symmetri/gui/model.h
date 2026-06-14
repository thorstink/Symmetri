#pragma once

#include <stddef.h>

#include <filesystem>
#include <functional>
#include <future>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
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

// node-type
enum class NodeType { Place, Transition };
// (source-node-type, transition idx, place idx)
using Arc = std::tuple<NodeType, size_t, size_t>;
// (node-type, node idx)
using Node = std::tuple<NodeType, size_t>;

// The undoable document: the net, its marking, layout and the event log. Edit
// reducers are pure EditState -> EditState transitions, so they can be
// snapshotted and replayed for undo/redo.
struct EditState {
  std::optional<std::filesystem::path> active_file;
  std::vector<Coordinate> t_positions, p_positions;
  std::vector<size_t> t_view, p_view;
  std::vector<symmetri::AugmentedToken> tokens;
  symmetri::SmallLog log;
  symmetri::Petri::PTNet net;
};

// Transient UI state: selection, highlighting, camera, popups. Mutated by view
// reducers and never recorded in the undo history (it survives undo/redo).
struct ViewState {
  ViewState();
  bool show_grid = true;
  Coordinate scrolling;
  std::optional<Coordinate> context_menu_pos;
  std::optional<Arc> selected_arc_idxs;
  std::optional<Node> selected_node_idx;
  float zoom_factor = 1.0f;
  std::map<Drawable, std::promise<void>> blockers;
  std::vector<size_t> t_highlight, p_highlight;
  std::vector<Arc> arc_highlight;
  bool arc_hovered = false;
  std::vector<std::string_view> colors = symmetri::Token::getColors();
  std::vector<Drawable> drawables;
};

struct Model {
  // Keep Model::NodeType/Arc/Node spellings working alongside model::NodeType.
  using NodeType = model::NodeType;
  using Arc = model::Arc;
  using Node = model::Node;

  Model() = default;
  // move-only: the model is threaded through the reducer pipeline by move and
  // should never be silently copied.
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;
  Model(Model&&) = default;
  Model& operator=(Model&&) = default;

  EditState edit;
  ViewState view;
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
  ViewModel(const Model& m);
  ViewModel(const ViewModel&) = delete;
  ViewModel& operator=(const ViewModel&) = delete;
  ViewModel(ViewModel&&) = default;
};

// Reducers come in two flavours:
//  - Edit reducers are pure EditState -> EditState transitions. They are the
//    only thing recorded in the undo history and must NOT read ViewState (that
//    would make replay diverge).
//  - View reducers mutate transient ViewState in place; they are never logged
//    and may read (but not write) the current EditState.
using EditReducer = std::function<EditState(EditState&&)>;
using ViewReducer = std::function<void(ViewState&, const EditState&)>;
// Async producer of an edit reducer (runs off the UI thread, e.g. file I/O).
using Computer = std::function<EditReducer(void)>;

// Reset every ViewState field that indexes into EditState (selection and
// highlights). Must be called whenever EditState changes structurally beneath
// the view (e.g. after undo/redo), otherwise those indices dangle into the net.
void clearSelection(ViewState& v);

// Undo/redo are plain messages through the same dispatch channel.
struct Undo {};
struct Redo {};

// The resolved unit of work applied at the reduce step.
using Action = std::variant<EditReducer, ViewReducer, Undo, Redo>;

// Undo history. Every edit reducer is appended to `log`; the cursor is the
// number of reducers currently applied. Undo/redo move the cursor and rebuild
// EditState by replaying the log; a new edit truncates any redo branch.
//
// NOTE: this is the replay-log half of the intended checkpoint+replay scheme,
// WITHOUT periodic snapshots: EditState cannot be copied while the net holds
// move-only symmetri::Callback values (net.store), so we always replay from the
// empty initial state. That makes undo O(number of edits). Re-introducing
// kSnapshot-every-N (and bounded undo cost) requires first making the editor's
// net representation copyable.
struct History {
  std::vector<EditReducer> log;
  size_t cursor = 0;

  bool canUndo() const { return cursor > 0; }
  bool canRedo() const { return cursor < log.size(); }

  // Apply a brand-new edit reducer, discarding any redo branch beyond cursor.
  EditState apply(EditReducer r, EditState&& current) {
    if (cursor < log.size()) log.resize(cursor);  // new edit after undo
    EditState next = r(std::move(current));
    log.push_back(std::move(r));
    ++cursor;
    return next;
  }

  EditState undo() {  // precondition: canUndo()
    --cursor;
    return materialize();
  }
  EditState redo() {  // precondition: canRedo()
    ++cursor;
    return materialize();
  }

 private:
  // Rebuild the EditState at `cursor` by replaying the log from empty.
  EditState materialize() {
    EditState s{};
    for (size_t i = 0; i < cursor; ++i) s = log[i](std::move(s));
    return s;
  }
};

}  // namespace model
