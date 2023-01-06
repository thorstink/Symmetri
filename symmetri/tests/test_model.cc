#include <spdlog/spdlog.h>

#include <catch2/catch_test_macros.hpp>
#include <iostream>

#include "model.h"
using namespace symmetri;

// global counters to keep track of how often the transitions are called.
unsigned int T0_COUNTER, T1_COUNTER;
// two transitions
void t0() { T0_COUNTER++; }
auto t1() {
  T1_COUNTER++;
  return symmetri::TransitionState::Completed;
}

std::tuple<StateNet, Store,
           std::vector<std::pair<symmetri::Transition, uint8_t>>, NetMarking>
testNet() {
  T0_COUNTER = 0;
  T1_COUNTER = 0;

  StateNet net = {{"t0", {{"Pa", "Pb"}, {"Pc"}}},
                  {"t1", {{"Pc", "Pc"}, {"Pb", "Pb", "Pd"}}}};

  Store store = {{"t0", &t0}, {"t1", &t1}};
  std::vector<std::pair<symmetri::Transition, uint8_t>> priority;

  NetMarking m0 = {{"Pa", 4}, {"Pb", 2}, {"Pc", 0}, {"Pd", 0}};
  return {net, store, priority, m0};
}

TEST_CASE("Test equaliy of nets") {
  auto [net, store, priority, m0] = testNet();
  auto net2 = net;
  auto net3 = net;
  REQUIRE(StateNetEquality(net, net2));
  // change net
  net2.insert({"tx", {{}, {}}});
  REQUIRE(!StateNetEquality(net, net2));

  // same transitions but different places.
  net3.at("t0").second.push_back("px");
  REQUIRE(!StateNetEquality(net, net3));
}

TEST_CASE("Create a model") {
  auto [net, store, priority, m0] = testNet();
  auto before_model_creation = clock_s::now();
  auto m = Model(net, store, priority, m0);
  auto after_model_creation = clock_s::now();
  REQUIRE(before_model_creation <= m.timestamp);
  REQUIRE(after_model_creation > m.timestamp);
}

TEST_CASE("Run a transition") {
  auto [net, store, priority, m0] = testNet();
  auto m = Model(net, store, priority, m0);

  // by "manually calling" a transition like this, we don't deduct the
  // pre-conditions from the marking.
  auto reducer = runTransition("t0", getTransition(m.store, "t0"), "nocase");

  // run the reducer
  m = reducer(std::move(m));

  // the reducer only processes the post-conditions on the marking, for t0 that
  // means  place Pc gets a +1.
  REQUIRE(std::count(m.tokens.begin(), m.tokens.end(), "Pc") == 1);
  REQUIRE(T0_COUNTER == 1);
}

TEST_CASE("Run one transition iteration in a petri net") {
  using namespace moodycamel;

  auto [net, store, priority, m0] = testNet();

  auto m = Model(net, store, priority, m0);
  BlockingConcurrentQueue<Reducer> reducers(4);
  StoppablePool stp(1);

  // t0 is enabled.
  m = runAll(m, reducers, stp);
  // t0 is dispatched but not yet run, so pre-conditions are processed but post
  // are not:
  REQUIRE(m.pending_transitions ==
          std::vector<symmetri::Transition>{"t0", "t0"});

  REQUIRE(MarkingEquality(m.tokens, {"Pa", "Pa"}));

  Reducer r;
  // there is no reducer yet because the task hasn't been executed yet.
  REQUIRE(!reducers.try_dequeue(r));
  stp.enqueue([] {});
  // there is an action to be dequed.
  // execture the actual task

  // now there should be two reducers;
  Reducer r1, r2;
  REQUIRE(reducers.wait_dequeue_timed(r1, std::chrono::seconds(1)));
  REQUIRE(reducers.wait_dequeue_timed(r2, std::chrono::seconds(1)));
  REQUIRE(T0_COUNTER == 2);
  // the marking should still be the same.
  REQUIRE(m.pending_transitions ==
          std::vector<symmetri::Transition>{"t0", "t0"});
  REQUIRE(MarkingEquality(m.tokens, {"Pa", "Pa"}));

  // process the reducers
  m = r1(std::move(m));
  m = r2(std::move(m));
  // and now the post-conditions are processed:
  REQUIRE(m.pending_transitions.empty());
  REQUIRE(MarkingEquality(m.tokens, {"Pa", "Pa", "Pc", "Pc"}));
  stp.stop();
}

TEST_CASE("Run until net dies") {
  using namespace moodycamel;

  auto [net, store, priority, m0] = testNet();
  auto m = Model(net, store, priority, m0);
  BlockingConcurrentQueue<Reducer> reducers(4);
  StoppablePool stp(1);
  Reducer r;
  PolyAction a([] {});
  // we need to enqueue one 'no-operation' to start the live net.
  reducers.enqueue([](Model&& m) -> Model& { return m; });
  do {
    if (reducers.try_dequeue(r)) {
      m = runAll(r(std::move(m)), reducers, stp);
    }
  } while (!m.pending_transitions.empty());
  // For this specific net we expect:
  REQUIRE(MarkingEquality(m.tokens, {"Pb", "Pb", "Pd", "Pd"}));

  REQUIRE(T0_COUNTER == 4);
  REQUIRE(T1_COUNTER == 2);
  stp.stop();
}

TEST_CASE("Run until net dies with std::nullopt") {
  using namespace moodycamel;

  auto [net, store, priority, m0] = testNet();
  store = {{"t0", std::nullopt}, {"t1", std::nullopt}};
  auto m = Model(net, store, priority, m0);
  BlockingConcurrentQueue<Reducer> reducers(4);
  StoppablePool stp(1);
  Reducer r;
  PolyAction a([] {});
  // we need to enqueue one 'no-operation' to start the live net.
  reducers.enqueue([](Model&& m) -> Model& { return m; });
  do {
    if (reducers.try_dequeue(r)) {
      m = runAll(r(std::move(m)), reducers, stp);
    }
  } while (!m.pending_transitions.empty());
  // For this specific net we expect:
  REQUIRE(MarkingEquality(m.tokens, {"Pb", "Pb", "Pd", "Pd"}));
  stp.stop();
}
