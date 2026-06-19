#pragma once

#include <stddef.h>

#include <filesystem>
#include <functional>
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

// A named, data-carrying popup/dialog. `id` gives it identity (used as the
// window title and to open/close/dedup it by name) while `draw` renders the
// popup body and may dispatch reducers. A bare function pointer can't carry
// data and a std::function can't be compared by identity; pairing an id string
// with a thunk gives both. Popups live in ViewState (transient, never
// snapshotted), so capturing data in `draw` is fine.
struct Popup {
  std::string id;
  std::function<void(const ViewModel&)> draw;
};

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

// A copyable, editor-local net. It mirrors symmetri::Petri::PTNet's fields, but
// replaces the move-only `store` (std::vector<symmetri::Callback>) with the
// only thing the editor needs from it: the token each transition outputs when
// fired
// (`fire(store[i])`). Being copyable is what lets EditState — and therefore the
// undo history — keep real snapshots.
struct Net {
  std::vector<std::string> place, transition;
  std::vector<symmetri::SmallVectorInput> input_n,
      output_n;  // (place id, colour)
  std::vector<symmetri::SmallVector> p_to_ts_n;
  std::vector<int8_t> priority;
  std::vector<symmetri::Token> output;  // per-transition fire() result
};

// The undoable document: the net, its marking, layout and the event log. Edit
// reducers are pure EditState -> EditState transitions, and EditState is now
// copyable, so the undo history can snapshot it (checkpoint + replay).
struct EditState {
  std::optional<std::filesystem::path> active_file;
  std::vector<Coordinate> t_positions, p_positions;
  std::vector<symmetri::AugmentedToken> tokens;
  symmetri::SmallLog log;
  Net net;
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
  std::vector<Popup> popups;
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
  const std::vector<Popup>& popups;
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
  const std::vector<size_t> t_highlight, p_highlight;
  const std::vector<Model::Arc> arc_highlight;
  const Coordinate node_size;
  const std::vector<std::string_view> colors;
  const std::vector<symmetri::AugmentedToken> tokens;
  const symmetri::SmallLog& log;

  const Net& net;
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

// Reset every ViewState field that indexes into EditState (selection and
// highlights). Must be called whenever EditState changes structurally beneath
// the view (e.g. after undo/redo), otherwise those indices dangle into the net.
void clearSelection(ViewState& v);

// Undo/redo are plain messages through the same dispatch channel.
struct Undo {};
struct Redo {};

// The resolved unit of work applied at the reduce step.
using Action = std::variant<EditReducer, ViewReducer, Undo, Redo>;

// A deferred computation run off the UI thread (e.g. file I/O, or any slow
// background task). Its result Action is dispatched back through the reducer
// pipeline when it finishes, so a background task can yield either an edit or a
// view reducer without blocking the view while it runs.
using Computer = std::function<Action(void)>;

// Undo history using checkpoint + replay: every edit reducer is appended to
// `log`, and a full EditState snapshot is kept every kSnapshotEvery steps. To
// reach step n we restore the nearest snapshot <= n and replay the logged
// reducers up to n -> undo/redo cost is bounded by kSnapshotEvery instead of
// the whole history. `cursor` is the number of reducers currently applied; a
// new edit truncates any redo branch beyond it.
struct History {
  static constexpr size_t kSnapshotEvery = 25;

  std::vector<EditReducer> log;
  std::vector<EditState> snapshots = std::vector<EditState>(1);  // [0] == empty
  size_t cursor = 0;

  bool canUndo() const { return cursor > 0; }
  bool canRedo() const { return cursor < log.size(); }

  // Apply a brand-new edit reducer, discarding any redo branch beyond cursor.
  EditState apply(EditReducer r, EditState&& current) {
    if (cursor < log.size()) {  // new edit after undo: drop the redo branch
      log.resize(cursor);
      snapshots.resize(cursor / kSnapshotEvery + 1);
    }
    EditState next = r(std::move(current));
    log.push_back(std::move(r));
    ++cursor;
    if (cursor % kSnapshotEvery == 0) snapshots.push_back(next);  // checkpoint
    return next;
  }

  EditState undo() { return materialize(--cursor); }  // precondition: canUndo()
  EditState redo() { return materialize(++cursor); }  // precondition: canRedo()

 private:
  // Rebuild EditState at `target` from the nearest earlier snapshot.
  EditState materialize(size_t target) {
    const size_t j = target / kSnapshotEvery;
    EditState s = snapshots[j];  // copy the checkpoint
    for (size_t i = j * kSnapshotEvery; i < target; ++i)
      s = log[i](std::move(s));
    return s;
  }
};

}  // namespace model
