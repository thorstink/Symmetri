#include <algorithm>
#include <string>
#include <vector>

#include "doctest/doctest.h"
#include "symmetri/symmetri.h"
#include "symmetri/utilities.hpp"

// Payload types. The type defines the token: each is bound 1:1 to a color.
struct Pose {
  double x, y;
};
using Path = std::vector<Pose>;
struct Report {
  std::string text;
};

CREATE_TYPED_TOKEN(PathToken, Path)
CREATE_TYPED_TOKEN(ReportToken, Report)
CREATE_TYPED_TOKEN(Counter, int)

using namespace symmetri;

TEST_CASE("typed color carries its payload to a color-matched parameter") {
  auto pool = std::make_shared<InlineExecutor>();
  // The arc colors are the contract: t0 consumes a PathToken, produces one.
  Net net = {{"t0", {{{"p_in", PathToken}}, {{"p_out", PathToken}}}}};
  Marking m0 = {{"p_in", Path{{1.0, 2.0}, {3.0, 4.0}}}};  // color inferred
  PetriNet app(net, "typed", pool, m0, {{"p_out", PathToken}});

  double last_x = 0.0;
  app.registerCallback("t0", [&last_x](const Path& p) -> Marking {
    last_x = p.back().x;
    return {{"p_out", p}};  // color inferred from the Path type
  });

  CHECK(fire(app) == Success);
  CHECK(last_x == 3.0);
}

TEST_CASE("matching is by color, not position: dataless tokens are skipped") {
  auto pool = std::make_shared<InlineExecutor>();
  // t0 consumes a Success guard token AND a Counter token — in that arc
  // order. The int parameter must match the Counter token regardless.
  Net net = {
      {"t0",
       {{{"p_go", Success}, {"p_count", Counter}}, {{"p_done", Success}}}}};
  Marking m0 = {{"p_go", Success}, {"p_count", 41}};
  PetriNet app(net, "skip_dataless", pool, m0, {{"p_done", Success}});

  int seen = 0;
  app.registerCallback("t0", [&seen](const int& count) {
    seen = count + 1;
    return Success;
  });

  CHECK(fire(app) == Success);
  CHECK(seen == 42);
}

TEST_CASE("two typed parameters bind independent of arc order") {
  auto pool = std::make_shared<InlineExecutor>();
  Net net = {
      {"t0",
       {{{"p_r", ReportToken}, {"p_p", PathToken}}, {{"p_out", Success}}}}};
  // Note: callback parameter order (Path first) is the REVERSE of arc order.
  Marking m0 = {{"p_r", Report{"ok"}}, {"p_p", Path{{5.0, 6.0}}}};
  PetriNet app(net, "order_free", pool, m0, {{"p_out", Success}});

  std::string combined;
  app.registerCallback("t0", [&combined](const Path& p, const Report& r) {
    combined = r.text + "@" + std::to_string(p.front().x);
    return Success;
  });

  CHECK(fire(app) == Success);
  CHECK(combined == "ok@5.000000");
}

TEST_CASE("payload on a dataless color throws TokenTypeError") {
  // The value-carrying constructor infers the color, so a payload on a
  // dataless color is only expressible via the pre-wrapped passthrough —
  // which validates the binding at runtime.
  CHECK_THROWS_AS(
      PlaceToken("p0", Success,
                 std::make_shared<const std::any>(Path{{1.0, 1.0}})),
      TokenTypeError);
}

TEST_CASE("wrong payload type on a typed color throws TokenTypeError") {
  CHECK_THROWS_AS(
      PlaceToken("p0", PathToken,
                 std::make_shared<const std::any>(Report{"not a path"})),
      TokenTypeError);
}

TEST_CASE("a bad deposit inside a callback fails the firing, not the app") {
  auto pool = std::make_shared<InlineExecutor>();
  Net net = {{"t0", {{{"p0", Success}}, {{"p1", Success}}}}};
  Marking m0 = {{"p0", Success}};
  PetriNet app(net, "bad_deposit", pool, m0, {});

  app.registerCallback("t0", []() -> Marking {
    // Success is dataless — this deposit is illegal and throws; the
    // dispatcher turns it into a Failed firing.
    return {
        {"p1", Success, std::make_shared<const std::any>(Path{{9.0, 9.0}})}};
  });

  fire(app);
  const auto marking = app.getMarking();
  REQUIRE(marking.size() == 1);
  CHECK(marking[0].place == "p1");
  CHECK(marking[0].color == Failed);
}

TEST_CASE("parameter without a matching consumed token fails the firing") {
  auto pool = std::make_shared<InlineExecutor>();
  // t0 only consumes a dataless Success token, but the callback wants a Path.
  Net net = {{"t0", {{{"p0", Success}}, {{"p1", Success}}}}};
  Marking m0 = {{"p0", Success}};
  PetriNet app(net, "no_match", pool, m0, {});

  app.registerCallback("t0", [](const Path&) { return Success; });

  fire(app);
  const auto marking = app.getMarking();
  REQUIRE(marking.size() == 1);
  CHECK(marking[0].color == Failed);
}

TEST_CASE("typed token relays across transitions through a typed arc") {
  auto pool = std::make_shared<InlineExecutor>();
  // t0 plans a path, t1 refines it: p_mid's arc color is the contract.
  Net net = {{"t0", {{{"p_go", Success}}, {{"p_mid", PathToken}}}},
             {"t1", {{{"p_mid", PathToken}}, {{"p_end", ReportToken}}}}};
  Marking m0 = {{"p_go", Success}};
  PetriNet app(net, "relay", pool, m0, {{"p_end", ReportToken}});

  app.registerCallback("t0", []() -> Marking {
    return {{"p_mid", Path{{1.0, 1.0}, {2.0, 2.0}}}};
  });
  app.registerCallback("t1", [](const Path& p) -> Marking {
    return {{"p_end", Report{std::to_string(p.size()) + " poses"}}};
  });

  CHECK(fire(app) == Success);
  const auto marking = app.getMarking();
  REQUIRE(marking.size() == 1);
  CHECK(marking[0].get<Report>().text == "2 poses");
}

TEST_CASE("deposits covering all output places are placed as given") {
  auto pool = std::make_shared<InlineExecutor>();
  Net net = {
      {"t0",
       {{{"p_in", Success}}, {{"p_report", ReportToken}, {"p_ok", Success}}}}};
  Marking m0 = {{"p_in", Success}};
  PetriNet app(net, "full_cover", pool, m0, {});

  // Both output places get a token; the branch is in the COLOR: p_ok gets a
  // Failed token, which no Success-colored arc downstream would consume.
  app.registerCallback("t0", []() -> Marking {
    return {{"p_report", Report{"r"}}, {"p_ok", Failed}};
  });

  fire(app);
  const auto marking = app.getMarking();
  REQUIRE(marking.size() == 2);
  CHECK(std::any_of(marking.begin(), marking.end(), [](const auto& pt) {
    return pt.place == "p_report" && pt.color == ReportToken;
  }));
  CHECK(std::any_of(marking.begin(), marking.end(), [](const auto& pt) {
    return pt.place == "p_ok" && pt.color == Failed;
  }));
}

TEST_CASE("a single-color marking stamps every output place (legacy)") {
  auto pool = std::make_shared<InlineExecutor>();
  Net net = {
      {"t0", {{{"p_in", Success}}, {{"p_a", Success}, {"p_b", Success}}}}};
  Marking m0 = {{"p_in", Success}};
  PetriNet app(net, "single_color", pool, m0, {});

  // One color, one entry, and its place name is not even an output: the
  // single-color rule kicks in and BOTH output places get a Failed token.
  // Omitting an output place is no longer a branch.
  app.registerCallback("t0", []() -> Marking { return {{"p_bogus", Failed}}; });

  fire(app);
  const auto marking = app.getMarking();
  REQUIRE(marking.size() == 2);
  CHECK(std::all_of(marking.begin(), marking.end(),
                    [](const auto& pt) { return pt.color == Failed; }));
}

TEST_CASE("a marking matching neither shape is ignored entirely") {
  auto pool = std::make_shared<InlineExecutor>();
  Net net = {
      {"t0", {{{"p_in", Success}}, {{"p_a", Success}, {"p_b", Success}}}}};
  Marking m0 = {{"p_in", Success}};
  PetriNet app(net, "mismatch_ignored", pool, m0, {});

  // Two distinct colors AND no exact cover of {p_a, p_b}: the whole marking
  // is dropped — neither p_a nor p_b receives anything.
  app.registerCallback("t0", []() -> Marking {
    return {{"p_a", Success}, {"p_bogus", Report{"stray"}}};
  });

  fire(app);
  CHECK(app.getMarking().empty());
}

TEST_CASE("raw-token shape still works and sees typed payloads") {
  auto pool = std::make_shared<InlineExecutor>();
  Net net = {{"t0", {{{"p0", Counter}}, {{"p1", Success}}}}};
  Marking m0 = {{"p0", 7}};
  PetriNet app(net, "raw", pool, m0, {{"p1", Success}});

  int seen = 0;
  app.registerCallback("t0",
                       [&seen](const std::vector<AugmentedToken>& consumed) {
                         seen = consumed.at(0).get<int>();
                         return Success;
                       });

  CHECK(fire(app) == Success);
  CHECK(seen == 7);
}

TEST_CASE("legacy nullary callbacks are untouched by the type machinery") {
  auto pool = std::make_shared<InlineExecutor>();
  Net net = {{"t0", {{{"p0", Counter}}, {{"p1", Success}}}}};
  Marking m0 = {{"p0", 1}};
  PetriNet app(net, "legacy", pool, m0, {{"p1", Success}});

  bool ran = false;
  app.registerCallback("t0", [&ran] { ran = true; });

  CHECK(fire(app) == Success);
  CHECK(ran);
}
