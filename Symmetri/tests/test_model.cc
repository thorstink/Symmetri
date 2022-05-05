#include <catch2/catch_test_macros.hpp>

#include "model.h"

using namespace symmetri;
using namespace moodycamel;
std::tuple<StateNet, Store, NetMarking> testNet() {
  StateNet net;
  net["t0"] = {{"Pa", "Pb"}, {"Pc"}};
  net["t1"] = {{"Pc"}, {"Pb", "Pd"}};

  Store store = {{"t0", [] { return symmetri::TransitionState::Completed; }},
                 {"t1", [] { return symmetri::TransitionState::Completed; }}};

  NetMarking m0;
  m0["Pa"] = 3;
  m0["Pb"] = 1;
  m0["Pc"] = 0;
  m0["Pd"] = 0;

  return {net, store, m0};
}

TEST_CASE("Create a model") {
  auto [net, store, m0] = testNet();
  auto before_model_creation = clock_s::now();
  auto m = Model(net, store, m0);
  auto after_model_creation = clock_s::now();
  REQUIRE(before_model_creation < m.timestamp);
  REQUIRE(after_model_creation > m.timestamp);
}

TEST_CASE("Run a transition") {
  auto [net, store, m0] = testNet();
  auto m = Model(net, store, m0);

  // by "manually calling" a transition like this, we don't deduct the
  // pre-conditions from the marking.
  auto reducer = runTransition("t0", store.at("t0"), "nocase");

  // run the reducer
  m = reducer(std::move(m));

  // the reducer only processes the post-conditions on the marking, for t0 that
  // means  place Pc gets a +1.
  REQUIRE(m.M["Pc"] == 1);
}

TEST_CASE("Run one transition iteration in a petri net") {
  auto [net, store, m0] = testNet();
  auto m = Model(net, store, m0);
  BlockingConcurrentQueue<Reducer> reducers(256);
  BlockingConcurrentQueue<PolyAction> polymorphic_actions(256);

  // t0 is enabled.
  m = runAll(m, reducers, polymorphic_actions, "a");
  // t0 is dispatched but not yet run, so pre-conditions are processed but post
  // are not:
  REQUIRE(m.pending_transitions == std::set<Transition>({"t0"}));
  REQUIRE(m.M == NetMarking({{"Pa", 2}, {"Pb", 0}, {"Pc", 0}, {"Pd", 0}}));
  Reducer r;
  // there is no reducer yet because the task hasn't been executed yet.
  REQUIRE(!reducers.try_dequeue(r));
  PolyAction a([] {});
  // there is an action to be dequed.
  REQUIRE(polymorphic_actions.try_dequeue(a));
  // execture the actual task
  run(a);
  // now there should be a reducer
  REQUIRE(reducers.try_dequeue(r));
  // the marking should still be the same.
  REQUIRE(m.pending_transitions == std::set<Transition>({"t0"}));
  REQUIRE(m.M == NetMarking({{"Pa", 2}, {"Pb", 0}, {"Pc", 0}, {"Pd", 0}}));
  // process the reducer
  m = r(std::move(m));
  // and now the post-conditions are processed:
  REQUIRE(m.pending_transitions.empty());
  REQUIRE(m.M == NetMarking({{"Pa", 2}, {"Pb", 0}, {"Pc", 1}, {"Pd", 0}}));
}
