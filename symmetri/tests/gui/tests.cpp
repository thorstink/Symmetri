#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

// Stubs for the draw-function pointers referenced by ViewState::ViewState()
// (model.cpp) and by view reducers that erase entries from drawables
// (reducers.cpp). These translation units are compiled into this test binary
// but their draw_*.cpp counterparts are not; we supply no-op definitions so
// the linker is satisfied without pulling in ImGui rendering.
#include "draw_context_menu.h"
#include "draw_graph.h"
#include "draw_menu_bar.h"
#include "draw_tools_menu.h"

void draw_menu_bar(const model::ViewModel&) {}
void draw_graph(const model::ViewModel&) {}
void draw_grid(const model::ViewModel&) {}
void draw_tools_menu(const model::ViewModel&) {}
void draw_context_menu(const model::ViewModel&) {}

#include <cstring>
#include <tuple>

#include "imgui.h"
#include "reducers.h"
#include "rxdispatch.h"
#include "symmetri/colors.hpp"

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------

namespace {

// Discard any items left in the queue from a previous test.
void clearQueue() {
  rxdispatch::Update u;
  while (rxdispatch::getQueue().try_dequeue(u)) {
  }
}

struct States {
  model::EditState edit;
  model::ViewState view;
};

// Pull everything out of the dispatch queue and apply reducers in FIFO order.
// Computers (background tasks) and Undo/Redo are skipped — they are not
// exercised by the direct reducer tests below.
States drain(States s = {}) {
  rxdispatch::Update u;
  while (rxdispatch::getQueue().try_dequeue(u)) {
    std::visit(
        [&s](auto&& r) {
          using T = std::decay_t<decltype(r)>;
          if constexpr (std::is_same_v<T, model::EditReducer>) {
            s.edit = r(std::move(s.edit));
          } else if constexpr (std::is_same_v<T, model::ViewReducer>) {
            r(s.view, s.edit);
          }
        },
        std::move(u));
  }
  return s;
}

// Build a minimal net with N places and M transitions (no arcs, no marking).
States makeNet(int n_places, int n_transitions) {
  States s;
  for (int i = 0; i < n_places; ++i) {
    s.edit.net.place.push_back("p" + std::to_string(i));
    s.edit.net.p_to_ts_n.push_back({});
    s.edit.p_positions.push_back({static_cast<float>(i) * 10.f, 0.f});
  }
  for (int i = 0; i < n_transitions; ++i) {
    s.edit.net.transition.push_back("t" + std::to_string(i));
    s.edit.net.input_n.push_back({});
    s.edit.net.output_n.push_back({});
    s.edit.net.priority.push_back(0);
    s.edit.net.output.push_back(symmetri::Success);
    s.edit.t_positions.push_back({static_cast<float>(i) * 10.f, 20.f});
  }
  return s;
}

}  // namespace

// ===========================================================================
// History
// ===========================================================================

TEST_CASE("history - apply appends to log and increments cursor") {
  model::History h;
  CHECK(!h.canUndo());
  CHECK(!h.canRedo());

  model::EditState e;
  e = h.apply(
      [](model::EditState&& s) {
        s.net.place.push_back("p0");
        return s;
      },
      std::move(e));

  CHECK(e.net.place.size() == 1);
  CHECK(h.cursor == 1);
  CHECK(h.log.size() == 1);
  CHECK(h.canUndo());
  CHECK(!h.canRedo());
}

TEST_CASE("history - undo/redo moves cursor and rebuilds state") {
  model::History h;
  model::EditState e;

  e = h.apply(
      [](model::EditState&& s) {
        s.net.place.push_back("p0");
        return s;
      },
      std::move(e));
  e = h.apply(
      [](model::EditState&& s) {
        s.net.place.push_back("p1");
        return s;
      },
      std::move(e));
  REQUIRE(e.net.place.size() == 2);

  e = h.undo();
  CHECK(e.net.place.size() == 1);
  CHECK(h.cursor == 1);
  CHECK(h.canUndo());
  CHECK(h.canRedo());

  e = h.undo();
  CHECK(e.net.place.empty());
  CHECK(h.cursor == 0);
  CHECK(!h.canUndo());
  CHECK(h.canRedo());

  e = h.redo();
  CHECK(e.net.place.size() == 1);

  e = h.redo();
  CHECK(e.net.place.size() == 2);
  CHECK(!h.canRedo());
}

TEST_CASE("history - new edit after undo discards redo branch") {
  model::History h;
  model::EditState e;

  e = h.apply(
      [](model::EditState&& s) {
        s.net.place.push_back("p0");
        return s;
      },
      std::move(e));
  e = h.apply(
      [](model::EditState&& s) {
        s.net.place.push_back("p1");
        return s;
      },
      std::move(e));
  e = h.undo();  // cursor = 1, redo branch = [r1]

  e = h.apply(
      [](model::EditState&& s) {
        s.net.transition.push_back("t0");
        return s;
      },
      std::move(e));

  CHECK(h.log.size() == 2);
  CHECK(h.cursor == 2);
  CHECK(!h.canRedo());
  CHECK(e.net.place.size() == 1);  // only p0 survived
  CHECK(e.net.transition.size() == 1);
}

TEST_CASE("history - snapshot taken every kSnapshotEvery steps") {
  model::History h;
  model::EditState e;

  CHECK(h.snapshots.size() == 1);  // [0] = initial empty state

  for (size_t i = 0; i < model::History::kSnapshotEvery; ++i)
    e = h.apply([](model::EditState&& s) { return s; }, std::move(e));

  CHECK(h.snapshots.size() == 2);

  for (size_t i = 0; i < model::History::kSnapshotEvery; ++i)
    e = h.apply([](model::EditState&& s) { return s; }, std::move(e));

  CHECK(h.snapshots.size() == 3);
}

// ===========================================================================
// Edit reducers
// ===========================================================================

TEST_CASE("addNode - creates a place with unique default name") {
  clearQueue();
  addNode(model::Model::NodeType::Place, ImVec2{10.f, 20.f});
  auto s = drain();

  REQUIRE(s.edit.net.place.size() == 1);
  CHECK(s.edit.net.place[0] == "place");
  CHECK(s.edit.net.p_to_ts_n.size() == 1);
  REQUIRE(s.edit.p_positions.size() == 1);
  CHECK(s.edit.p_positions[0].x == doctest::Approx(10.f));
  CHECK(s.edit.p_positions[0].y == doctest::Approx(20.f));
}

TEST_CASE("addNode - creates a transition with unique default name") {
  clearQueue();
  addNode(model::Model::NodeType::Transition, ImVec2{5.f, 15.f});
  auto s = drain();

  REQUIRE(s.edit.net.transition.size() == 1);
  CHECK(s.edit.net.transition[0] == "transition");
  CHECK(s.edit.net.input_n.size() == 1);
  CHECK(s.edit.net.output_n.size() == 1);
  CHECK(s.edit.net.priority.size() == 1);
  CHECK(s.edit.net.output.size() == 1);
  REQUIRE(s.edit.t_positions.size() == 1);
  CHECK(s.edit.t_positions[0].x == doctest::Approx(5.f));
  CHECK(s.edit.t_positions[0].y == doctest::Approx(15.f));
}

TEST_CASE("addNode - successive nodes get unique names") {
  clearQueue();
  addNode(model::Model::NodeType::Place, ImVec2{0.f, 0.f});
  auto s = drain();
  addNode(model::Model::NodeType::Place, ImVec2{0.f, 0.f});
  s = drain(std::move(s));

  REQUIRE(s.edit.net.place.size() == 2);
  CHECK(s.edit.net.place[0] != s.edit.net.place[1]);
}

TEST_CASE("addArc - place→transition adds to input_n and p_to_ts_n") {
  clearQueue();
  auto s = makeNet(1, 1);

  addArc(model::Model::NodeType::Place, 0 /*source=p0*/, 0 /*target=t0*/,
         symmetri::Success);
  s = drain(std::move(s));

  REQUIRE(s.edit.net.input_n[0].size() == 1);
  CHECK(std::get<size_t>(s.edit.net.input_n[0][0]) == 0);
  CHECK(std::get<symmetri::Token>(s.edit.net.input_n[0][0]) ==
        symmetri::Success);
  REQUIRE(s.edit.net.p_to_ts_n[0].size() == 1);
  CHECK(s.edit.net.p_to_ts_n[0][0] == 0);
}

TEST_CASE("addArc - transition→place adds to output_n only") {
  clearQueue();
  auto s = makeNet(1, 1);

  addArc(model::Model::NodeType::Transition, 0 /*source=t0*/, 0 /*target=p0*/,
         symmetri::Success);
  s = drain(std::move(s));

  REQUIRE(s.edit.net.output_n[0].size() == 1);
  CHECK(std::get<size_t>(s.edit.net.output_n[0][0]) == 0);
  CHECK(s.edit.net.input_n[0].empty());
}

TEST_CASE("addArc - p_to_ts_n deduplicates transition entries") {
  clearQueue();
  auto s = makeNet(1, 1);

  addArc(model::Model::NodeType::Place, 0, 0, symmetri::Success);
  s = drain(std::move(s));
  addArc(model::Model::NodeType::Place, 0, 0, symmetri::Success);
  s = drain(std::move(s));

  // Two arcs in input_n is fine, but p_to_ts_n[0] must not duplicate t0.
  CHECK(s.edit.net.p_to_ts_n[0].size() == 1);
}

TEST_CASE("removeArc - removes from input_n (place→transition)") {
  clearQueue();
  auto s = makeNet(1, 1);
  s.edit.net.input_n[0].push_back({0, symmetri::Success});

  removeArc(model::Model::NodeType::Place, 0 /*t_idx*/, 0 /*sub_idx*/);
  s = drain(std::move(s));

  CHECK(s.edit.net.input_n[0].empty());
}

TEST_CASE("removeArc - removes from output_n (transition→place)") {
  clearQueue();
  auto s = makeNet(1, 1);
  s.edit.net.output_n[0].push_back({0, symmetri::Success});

  removeArc(model::Model::NodeType::Transition, 0 /*t_idx*/, 0 /*sub_idx*/);
  s = drain(std::move(s));

  CHECK(s.edit.net.output_n[0].empty());
}

TEST_CASE("removePlace - erases place, position, and p_to_ts_n entry") {
  clearQueue();
  auto s = makeNet(2, 0);  // p0, p1

  removePlace(0);
  s = drain(std::move(s));

  REQUIRE(s.edit.net.place.size() == 1);
  CHECK(s.edit.net.place[0] == "p1");
  CHECK(s.edit.p_positions.size() == 1);
  CHECK(s.edit.net.p_to_ts_n.size() == 1);
}

TEST_CASE(
    "removePlace - drops arcs to removed place and reindexes higher ids") {
  clearQueue();
  auto s = makeNet(2, 1);  // p0, p1, t0

  // Arc from p1 to t0 — after removing p0, p1 becomes p0, so arc id shifts to
  // 0.
  s.edit.net.input_n[0].push_back({1, symmetri::Success});
  s.edit.net.p_to_ts_n[1].push_back(0);
  // Token in p1.
  s.edit.tokens.push_back({1, symmetri::Success});

  removePlace(0);  // remove p0
  s = drain(std::move(s));

  REQUIRE(s.edit.net.place.size() == 1);
  // Arc to old p1 (now p0) must be at index 0.
  REQUIRE(s.edit.net.input_n[0].size() == 1);
  CHECK(std::get<size_t>(s.edit.net.input_n[0][0]) == 0);
  // Token for old p1 must shift to index 0.
  REQUIRE(s.edit.tokens.size() == 1);
  CHECK(std::get<size_t>(s.edit.tokens[0]) == 0);
}

TEST_CASE("removePlace - drops arcs and tokens referencing the removed place") {
  clearQueue();
  auto s = makeNet(2, 1);  // p0, p1, t0

  s.edit.net.input_n[0].push_back({0, symmetri::Success});  // arc from p0
  s.edit.net.p_to_ts_n[0].push_back(0);
  s.edit.tokens.push_back({0, symmetri::Success});  // token in p0

  removePlace(0);  // remove p0 — arc and token must be erased
  s = drain(std::move(s));

  CHECK(s.edit.net.input_n[0].empty());
  CHECK(s.edit.tokens.empty());
}

TEST_CASE("removeTransition - erases all parallel arrays") {
  clearQueue();
  auto s = makeNet(0, 2);  // t0, t1

  removeTransition(0);
  s = drain(std::move(s));

  REQUIRE(s.edit.net.transition.size() == 1);
  CHECK(s.edit.net.transition[0] == "t1");
  CHECK(s.edit.net.input_n.size() == 1);
  CHECK(s.edit.net.output_n.size() == 1);
  CHECK(s.edit.net.priority.size() == 1);
  CHECK(s.edit.net.output.size() == 1);
  CHECK(s.edit.t_positions.size() == 1);
}

TEST_CASE("removeTransition - reindexes p_to_ts_n") {
  clearQueue();
  auto s = makeNet(1, 2);  // p0 connected to t0 and t1

  s.edit.net.p_to_ts_n[0].push_back(0);  // p0 → t0
  s.edit.net.p_to_ts_n[0].push_back(1);  // p0 → t1

  removeTransition(0);  // remove t0 — t1 becomes t0
  s = drain(std::move(s));

  REQUIRE(s.edit.net.p_to_ts_n[0].size() == 1);
  CHECK(s.edit.net.p_to_ts_n[0][0] == 0);  // old t1, now t0
}

TEST_CASE("moveNode - updates positions by delta") {
  clearQueue();
  auto s = makeNet(1, 1);
  // Override positions to known values so the test is independent of makeNet.
  s.edit.t_positions[0] = {100.f, 200.f};
  s.edit.p_positions[0] = {50.f, 60.f};

  moveNode({0}, {0}, ImVec2{5.f, 10.f});
  s = drain(std::move(s));

  CHECK(s.edit.t_positions[0].x == doctest::Approx(105.f));
  CHECK(s.edit.t_positions[0].y == doctest::Approx(210.f));
  CHECK(s.edit.p_positions[0].x == doctest::Approx(55.f));
  CHECK(s.edit.p_positions[0].y == doctest::Approx(70.f));
}

TEST_CASE("addTokenToPlace - appends token") {
  clearQueue();
  auto s = makeNet(1, 0);

  addTokenToPlace({0, symmetri::Success});
  s = drain(std::move(s));

  REQUIRE(s.edit.tokens.size() == 1);
  CHECK(std::get<size_t>(s.edit.tokens[0]) == 0);
}

TEST_CASE("removeTokenFromPlace - removes first matching token") {
  clearQueue();
  auto s = makeNet(1, 0);
  s.edit.tokens.push_back({0, symmetri::Success});
  s.edit.tokens.push_back({0, symmetri::Success});

  removeTokenFromPlace({0, symmetri::Success});
  s = drain(std::move(s));

  CHECK(s.edit.tokens.size() == 1);
}

TEST_CASE(
    "tryFire - no-input transition always fires and produces a log entry") {
  clearQueue();
  auto s = makeNet(0, 1);  // t0 with no input arcs

  tryFire(0);
  s = drain(std::move(s));

  CHECK(s.edit.log.size() == 1);
  CHECK(s.edit.log[0].transition == 0);
  CHECK(s.edit.tokens.empty());  // no output arcs either
}

TEST_CASE("tryFire - enabled transition fires and updates marking") {
  clearQueue();
  auto s = makeNet(2, 1);  // p0, p1, t0

  // p0 → t0 → p1
  s.edit.net.input_n[0].push_back({0, symmetri::Success});
  s.edit.net.output_n[0].push_back({1, symmetri::Success});
  s.edit.tokens.push_back({0, symmetri::Success});  // enable t0

  tryFire(0);
  s = drain(std::move(s));

  REQUIRE(s.edit.log.size() == 1);
  // Token was consumed from p0 and produced in p1.
  REQUIRE(s.edit.tokens.size() == 1);
  CHECK(std::get<size_t>(s.edit.tokens[0]) == 1);
}

TEST_CASE("tryFire - disabled transition does not fire") {
  clearQueue();
  auto s = makeNet(1, 1);  // p0, t0; p0 → t0 but no token

  s.edit.net.input_n[0].push_back({0, symmetri::Success});

  tryFire(0);
  s = drain(std::move(s));

  CHECK(s.edit.log.empty());
  CHECK(s.edit.tokens.empty());
}

TEST_CASE("updateTransitionOutputColor - changes output token color") {
  clearQueue();
  auto s = makeNet(0, 1);

  CHECK(s.edit.net.output[0] == symmetri::Success);
  updateTransitionOutputColor(0, symmetri::Failed);
  s = drain(std::move(s));

  CHECK(s.edit.net.output[0] == symmetri::Failed);
}

TEST_CASE("updateArcColor - changes color on input arc") {
  clearQueue();
  auto s = makeNet(1, 1);
  s.edit.net.input_n[0].push_back({0, symmetri::Success});

  updateArcColor(model::Model::NodeType::Place, 0, 0, symmetri::Failed);
  s = drain(std::move(s));

  CHECK(std::get<symmetri::Token>(s.edit.net.input_n[0][0]) ==
        symmetri::Failed);
}

TEST_CASE("updateArcColor - changes color on output arc") {
  clearQueue();
  auto s = makeNet(1, 1);
  s.edit.net.output_n[0].push_back({0, symmetri::Success});

  updateArcColor(model::Model::NodeType::Transition, 0, 0, symmetri::Failed);
  s = drain(std::move(s));

  CHECK(std::get<symmetri::Token>(s.edit.net.output_n[0][0]) ==
        symmetri::Failed);
}

TEST_CASE("updatePlaceName - renames place via callback") {
  clearQueue();
  auto s = makeNet(1, 0);

  size_t idx = 0;
  char buf[] = "renamed";
  ImGuiInputTextCallbackData data{};
  data.Buf = buf;
  data.BufTextLen = static_cast<int>(std::strlen(buf));
  data.UserData = &idx;

  updatePlaceName(&data);
  s = drain(std::move(s));

  CHECK(s.edit.net.place[0] == "renamed");
}

TEST_CASE("updateTransitionName - renames transition via callback") {
  clearQueue();
  auto s = makeNet(0, 1);

  size_t raw = 1000 + 0;  // encoding: idx + 1000
  char buf[] = "fire_missiles";
  ImGuiInputTextCallbackData data{};
  data.Buf = buf;
  data.BufTextLen = static_cast<int>(std::strlen(buf));
  data.UserData = &raw;

  updateTransitionName(&data);
  s = drain(std::move(s));

  CHECK(s.edit.net.transition[0] == "fire_missiles");
}

TEST_CASE("updatePriority - valid string sets priority") {
  clearQueue();
  auto s = makeNet(0, 1);

  size_t idx = 0;
  char buf[] = "42";
  ImGuiInputTextCallbackData data{};
  data.Buf = buf;
  data.BufTextLen = static_cast<int>(std::strlen(buf));
  data.UserData = &idx;

  updatePriority(&data);
  s = drain(std::move(s));

  CHECK(s.edit.net.priority[0] == 42);
}

TEST_CASE("updatePriority - empty or minus-only string is a no-op") {
  clearQueue();
  auto s = makeNet(0, 1);

  size_t idx = 0;
  // Empty buf
  {
    char buf[] = "";
    ImGuiInputTextCallbackData data{};
    data.Buf = buf;
    data.BufTextLen = 0;
    data.UserData = &idx;
    updatePriority(&data);
  }
  // Just a minus sign
  {
    char buf[] = "-";
    ImGuiInputTextCallbackData data{};
    data.Buf = buf;
    data.BufTextLen = 1;
    data.UserData = &idx;
    updatePriority(&data);
  }
  s = drain(std::move(s));

  CHECK(s.edit.net.priority[0] == 0);  // unchanged
}

TEST_CASE("updateActiveFile - sets active_file via callback") {
  clearQueue();
  auto s = makeNet(0, 0);

  char buf[] = "/tmp/test.pnml";
  ImGuiInputTextCallbackData data{};
  data.Buf = buf;
  data.BufTextLen = static_cast<int>(std::strlen(buf));

  updateActiveFile(&data);
  s = drain(std::move(s));

  REQUIRE(s.edit.active_file.has_value());
  CHECK(s.edit.active_file->string() == "/tmp/test.pnml");
}

// ===========================================================================
// View reducers
// ===========================================================================

TEST_CASE("showGrid - toggles grid flag") {
  clearQueue();
  States s;
  CHECK(s.view.show_grid == true);

  showGrid(false);
  s = drain(std::move(s));
  CHECK(s.view.show_grid == false);

  showGrid(true);
  s = drain(std::move(s));
  CHECK(s.view.show_grid == true);
}

TEST_CASE("setArcHoverState") {
  clearQueue();
  States s;
  setArcHoverState(true);
  s = drain(std::move(s));
  CHECK(s.view.arc_hovered == true);

  setArcHoverState(false);
  s = drain(std::move(s));
  CHECK(s.view.arc_hovered == false);
}

TEST_CASE("zoomRelative - adjusts zoom_factor by delta") {
  clearQueue();
  States s;
  const float initial = s.view.zoom_factor;

  zoomRelative(0.5f);
  s = drain(std::move(s));
  CHECK(s.view.zoom_factor == doctest::Approx(initial + 0.5f));
}

TEST_CASE("zoomAbsolute - sets zoom_factor to exact value") {
  clearQueue();
  States s;

  zoomAbsolute(2.5f);
  s = drain(std::move(s));
  CHECK(s.view.zoom_factor == doctest::Approx(2.5f));
}

TEST_CASE("moveView - scrolls by delta / zoom_factor") {
  clearQueue();
  States s;
  s.view.zoom_factor = 2.0f;

  moveView(ImVec2{4.f, 6.f});
  s = drain(std::move(s));

  CHECK(s.view.scrolling.x == doctest::Approx(4.f / 2.f));
  CHECK(s.view.scrolling.y == doctest::Approx(6.f / 2.f));
}

TEST_CASE("setContextMenuActive/Inactive") {
  clearQueue();
  States s;

  setContextMenuActive(ImVec2{100.f, 200.f});
  s = drain(std::move(s));
  REQUIRE(s.view.context_menu_pos.has_value());
  CHECK(s.view.context_menu_pos->x == doctest::Approx(100.f));

  setContextMenuInactive();
  s = drain(std::move(s));
  CHECK(!s.view.context_menu_pos.has_value());
}

TEST_CASE("setSelectedNode - sets selection and clears others") {
  clearQueue();
  States s;
  s.view.t_highlight = {0, 1};
  s.view.p_highlight = {2};

  setSelectedNode(model::Model::NodeType::Transition, 3);
  s = drain(std::move(s));

  REQUIRE(s.view.selected_node_idx.has_value());
  CHECK(std::get<model::NodeType>(*s.view.selected_node_idx) ==
        model::NodeType::Transition);
  CHECK(std::get<size_t>(*s.view.selected_node_idx) == 3);
  CHECK(s.view.p_highlight.empty());
  CHECK(s.view.t_highlight.size() == 1);  // only idx 3 remains
  CHECK(s.view.t_highlight[0] == 3);
  CHECK(!s.view.selected_arc_idxs.has_value());
}

TEST_CASE("addHighlightNode - adds without replacing existing highlights") {
  clearQueue();
  States s;
  s.view.t_highlight = {0};

  addHighlightNode(model::Model::NodeType::Transition, 1);
  s = drain(std::move(s));

  REQUIRE(s.view.t_highlight.size() == 2);
  CHECK(s.view.t_highlight[1] == 1);
}

TEST_CASE("addHighlightNode - does not duplicate existing entry") {
  clearQueue();
  States s;
  s.view.t_highlight = {0};

  addHighlightNode(model::Model::NodeType::Transition, 0);
  s = drain(std::move(s));

  CHECK(s.view.t_highlight.size() == 1);
}

TEST_CASE("setSelectedTargetNode - replaces highlight for that type") {
  clearQueue();
  States s;
  s.view.p_highlight = {0, 1};

  setSelectedTargetNode(model::Model::NodeType::Place, 5);
  s = drain(std::move(s));

  REQUIRE(s.view.p_highlight.size() == 1);
  CHECK(s.view.p_highlight[0] == 5);
}

TEST_CASE("resetSelectedTargetNode - clears selected_arc_idxs") {
  clearQueue();
  States s;
  s.view.selected_arc_idxs = {model::NodeType::Place, 0, 0};

  resetSelectedTargetNode();
  s = drain(std::move(s));

  CHECK(!s.view.selected_arc_idxs.has_value());
}

TEST_CASE("resetSelection - clears all selection state") {
  clearQueue();
  States s;
  s.view.selected_node_idx = {model::NodeType::Place, 0};
  s.view.t_highlight = {0};
  s.view.p_highlight = {1};
  s.view.selected_arc_idxs = {model::NodeType::Transition, 0, 0};
  s.view.arc_highlight.emplace_back(model::NodeType::Place, 0, 0);

  resetSelection();
  s = drain(std::move(s));

  CHECK(!s.view.selected_node_idx.has_value());
  CHECK(s.view.t_highlight.empty());
  CHECK(s.view.p_highlight.empty());
  CHECK(!s.view.selected_arc_idxs.has_value());
  CHECK(s.view.arc_highlight.empty());
}

TEST_CASE("addHighlightArc - appends arc to highlight list") {
  clearQueue();
  States s;

  addHighlightArc(model::Model::NodeType::Place, 1, 2);
  s = drain(std::move(s));

  REQUIRE(s.view.arc_highlight.size() == 1);
  CHECK(std::get<model::NodeType>(s.view.arc_highlight[0]) ==
        model::NodeType::Place);
  CHECK(std::get<1>(s.view.arc_highlight[0]) == 1);
  CHECK(std::get<2>(s.view.arc_highlight[0]) == 2);
}

TEST_CASE("setSelectedArc - selects arc and clears node selection") {
  clearQueue();
  States s;
  s.view.selected_node_idx = {model::NodeType::Place, 0};
  s.view.t_highlight = {0};
  s.view.p_highlight = {1};

  setSelectedArc(model::Model::NodeType::Transition, 0, 1);
  s = drain(std::move(s));

  REQUIRE(s.view.selected_arc_idxs.has_value());
  CHECK(!s.view.selected_node_idx.has_value());
  CHECK(s.view.t_highlight.empty());
  CHECK(s.view.p_highlight.empty());
  REQUIRE(s.view.arc_highlight.size() == 1);
}

TEST_CASE("addPopup - inserts popup and deduplicates by id") {
  clearQueue();
  States s;

  addPopup("test", [](const model::ViewModel&) {});
  s = drain(std::move(s));
  REQUIRE(s.view.popups.size() == 1);
  CHECK(s.view.popups[0].id == "test");

  // Adding the same id again must replace (not duplicate).
  addPopup("test", [](const model::ViewModel&) {});
  s = drain(std::move(s));
  CHECK(s.view.popups.size() == 1);
}

TEST_CASE("removePopup - removes popup by id") {
  clearQueue();
  States s;
  s.view.popups.push_back({"keep", nullptr});
  s.view.popups.push_back({"remove", nullptr});

  removePopup("remove");
  s = drain(std::move(s));

  REQUIRE(s.view.popups.size() == 1);
  CHECK(s.view.popups[0].id == "keep");
}

TEST_CASE("updateColorTable - populates colors from Token registry") {
  clearQueue();
  States s;
  s.view.colors.clear();

  updateColorTable();
  s = drain(std::move(s));

  CHECK(!s.view.colors.empty());
}

TEST_CASE("resetNetView - centers scrolling on the bounding box of nodes") {
  clearQueue();
  States s = makeNet(1, 1);
  s.edit.p_positions[0] = {200.f, 300.f};
  s.edit.t_positions[0] = {100.f, 50.f};

  resetNetView();
  s = drain(std::move(s));

  // min_x = 100, min_y = 50; expected scrolling = {400-100, 120-50} = {300, 70}
  CHECK(s.view.scrolling.x == doctest::Approx(300.f));
  CHECK(s.view.scrolling.y == doctest::Approx(70.f));
}

// ===========================================================================
// model::clearSelection
// ===========================================================================

TEST_CASE("clearSelection - resets all view selection state") {
  model::ViewState v;
  v.selected_node_idx = {model::NodeType::Transition, 1};
  v.selected_arc_idxs = {model::NodeType::Place, 0, 0};
  v.t_highlight = {0, 1};
  v.p_highlight = {2};
  v.arc_highlight.emplace_back(model::NodeType::Place, 0, 0);

  model::clearSelection(v);

  CHECK(!v.selected_node_idx.has_value());
  CHECK(!v.selected_arc_idxs.has_value());
  CHECK(v.t_highlight.empty());
  CHECK(v.p_highlight.empty());
  CHECK(v.arc_highlight.empty());
}

// ===========================================================================
// deleteSelected
// ===========================================================================

TEST_CASE("deleteSelected - removes transitions in descending index order") {
  // Highlights {0, 2} are NOT already sorted descending.  deleteSelected must
  // sort them so t2 is removed first (net shrinks from the right), keeping t0's
  // index valid when it is removed next.  t1 (index 1, not highlighted) must
  // survive and end up as the only transition.
  clearQueue();
  States s = makeNet(0, 3);  // t0, t1, t2

  deleteSelected({0, 2}, {});
  s = drain(std::move(s));

  REQUIRE(s.edit.net.transition.size() == 1);
  CHECK(s.edit.net.transition[0] == "t1");
  CHECK(s.edit.t_positions.size() == 1);
}

TEST_CASE("deleteSelected - removes places in descending index order") {
  // Same invariant on the place side: {0, 2} sorted descending → p2 removed
  // first so p0's index is still valid on the second pass.  p1 must survive.
  clearQueue();
  States s = makeNet(3, 0);  // p0, p1, p2

  deleteSelected({}, {0, 2});
  s = drain(std::move(s));

  REQUIRE(s.edit.net.place.size() == 1);
  CHECK(s.edit.net.place[0] == "p1");
  CHECK(s.edit.p_positions.size() == 1);
}

TEST_CASE("deleteSelected - removes transitions before places") {
  // Transition removal erases rows from input_n/output_n but never touches
  // net.place or p_positions, so place indices in p_highlight remain valid
  // throughout the transition-removal pass.  This test documents that
  // invariant: with t1 and p1 both highlighted, the final net contains exactly
  // t0 and p0.
  clearQueue();
  States s = makeNet(2, 2);  // p0, p1; t0, t1

  deleteSelected({1}, {1});
  s = drain(std::move(s));

  REQUIRE(s.edit.net.transition.size() == 1);
  CHECK(s.edit.net.transition[0] == "t0");
  REQUIRE(s.edit.net.place.size() == 1);
  CHECK(s.edit.net.place[0] == "p0");
}

TEST_CASE("deleteSelected - no-op on empty selection") {
  clearQueue();
  States s = makeNet(2, 2);

  deleteSelected({}, {});
  // Nothing should be enqueued.
  rxdispatch::Update u;
  CHECK(!rxdispatch::getQueue().try_dequeue(u));
  // Net is unchanged.
  CHECK(s.edit.net.transition.size() == 2);
  CHECK(s.edit.net.place.size() == 2);
}
