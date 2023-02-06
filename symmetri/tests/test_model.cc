#include <catch2/catch_test_macros.hpp>
#include <iostream>

#include "model.h"
using namespace symmetri;
using namespace moodycamel;

// global counters to keep track of how often the transitions are called.
std::atomic<unsigned int> T0_COUNTER, T1_COUNTER;
// two transitions
void t0() {
  auto inc = T0_COUNTER.load() + 1;
  T0_COUNTER.store(inc);
}
auto t1() {
  auto inc = T1_COUNTER.load() + 1;
  T1_COUNTER.store(inc);
  return symmetri::TransitionState::Completed;
}

std::tuple<StateNet, Store,
           std::vector<std::pair<symmetri::Transition, int8_t>>, NetMarking>
testNet() {
  T0_COUNTER.store(0);
  T1_COUNTER.store(0);

  StateNet net = {{"t0", {{"Pa", "Pb"}, {"Pc"}}},
                  {"t1", {{"Pc", "Pc"}, {"Pb", "Pb", "Pd"}}}};

  Store store = {{"t0", &t0}, {"t1", &t1}};
  std::vector<std::pair<symmetri::Transition, int8_t>> priority;

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
  Model m(net, store, priority, m0);
  auto after_model_creation = clock_s::now();
  REQUIRE(before_model_creation <= m.timestamp);
  REQUIRE(after_model_creation > m.timestamp);
}

TEST_CASE("Run one transition iteration in a petri net") {
  auto [net, store, priority, m0] = testNet();

  Model m(net, store, priority, m0);
  BlockingConcurrentQueue<Reducer> reducers(4);
  StoppablePool stp(1);

  // t0 is enabled.
  m.runTransitions(reducers, stp, true, "");
  // t0 is dispatched but it's reducer has not yet run, so pre-conditions are
  // processed but post are not:
  REQUIRE(m.getActiveTransitions() ==
          std::vector<symmetri::Transition>{"t0", "t0"});
  REQUIRE(m.getMarking() == std::vector<symmetri::Place>{"Pa", "Pa"});

  // now there should be two reducers;
  Reducer r1, r2;
  REQUIRE(reducers.wait_dequeue_timed(r1, std::chrono::seconds(1)));
  REQUIRE(reducers.wait_dequeue_timed(r2, std::chrono::seconds(1)));
  // verify that t0 has actually ran twice.
  REQUIRE(T0_COUNTER == 2);
  // the marking should still be the same.
  REQUIRE(m.getMarking() == std::vector<symmetri::Place>{"Pa", "Pa"});

  CHECK(m.getActiveTransitions() ==
        std::vector<symmetri::Transition>{"t0", "t0"});

  // process the reducers
  m = r1(std::move(m));
  m = r2(std::move(m));
  // and now the post-conditions are processed:
  REQUIRE(m.active_transitions_n.empty());
  std::cout << std::endl;
  REQUIRE(MarkingEquality(
      m.getMarking(), std::vector<symmetri::Place>{"Pa", "Pa", "Pc", "Pc"}));
  stp.stop();
}

TEST_CASE("Run until net dies") {
  using namespace moodycamel;

  auto [net, store, priority, m0] = testNet();
  Model m(net, store, priority, m0);

  BlockingConcurrentQueue<Reducer> reducers(4);
  StoppablePool stp(1);
  Reducer r;
  PolyAction a([] {});
  // we need to enqueue one 'no-operation' to start the live net.
  reducers.enqueue([](Model&& m) -> Model& { return m; });
  do {
    if (reducers.try_dequeue(r)) {
      m = r(std::move(m));
      m.runTransitions(reducers, stp, true);
    }
  } while (m.active_transitions_n.size() > 0);

  // For this specific net we expect:
  REQUIRE(MarkingEquality(
      m.getMarking(), std::vector<symmetri::Place>{"Pb", "Pb", "Pd", "Pd"}));

  REQUIRE(T0_COUNTER.load() == 4);
  REQUIRE(T1_COUNTER.load() == 2);
  stp.stop();
}

TEST_CASE("Run until net dies with nullptr") {
  using namespace moodycamel;

  auto [net, store, priority, m0] = testNet();
  store = {{"t0", nullptr}, {"t1", nullptr}};
  Model m(net, store, priority, m0);

  BlockingConcurrentQueue<Reducer> reducers(4);
  StoppablePool stp(1);
  Reducer r;
  PolyAction a([] {});
  // we need to enqueue one 'no-operation' to start the live net.
  reducers.enqueue([](Model&& m) -> Model& { return m; });
  do {
    if (reducers.try_dequeue(r)) {
      m = r(std::move(m));
      m.runTransitions(reducers, stp, true);
    }
  } while (m.active_transitions_n.size() > 0);

  // For this specific net we expect:
  REQUIRE(MarkingEquality(
      m.getMarking(), std::vector<symmetri::Place>{"Pb", "Pb", "Pd", "Pd"}));
  stp.stop();
}

TEST_CASE(
    "getFireableTransitions return a vector of unique of unique fireable "
    "transitions") {
  StateNet net = {{"a", {{"Pa"}, {}}},
                  {"b", {{"Pa"}, {}}},
                  {"c", {{"Pa"}, {}}},
                  {"d", {{"Pa"}, {}}},
                  {"e", {{"Pb"}, {}}}};

  Store store;
  for (auto [t, dm] : net) {
    store.insert({t, nullptr});
  }
  // with this initial marking, all but transition e are possible.
  NetMarking m0 = {{"Pa", 1}};
  Model m(net, store, {}, m0);
  auto fireable_transitions = m.getFireableTransitions();
  auto find = [&](auto a) {
    return std::find(fireable_transitions.begin(), fireable_transitions.end(),
                     a);
  };
  REQUIRE(find("a") != fireable_transitions.end());
  REQUIRE(find("b") != fireable_transitions.end());
  REQUIRE(find("c") != fireable_transitions.end());
  REQUIRE(find("d") != fireable_transitions.end());
  REQUIRE(find("e") == fireable_transitions.end());
}

TEST_CASE("Step through transitions") {
  StateNet net = {{"a", {{"Pa"}, {}}},
                  {"b", {{"Pa"}, {}}},
                  {"c", {{"Pa"}, {}}},
                  {"d", {{"Pa"}, {}}},
                  {"e", {{"Pb"}, {}}}};

  std::map<std::string, size_t> hitmap;

  Store store;
  for (const auto& [t, dm] : net) {
    hitmap.insert({t, 0});
    store.insert({t, [&, t = t] { hitmap[t] += 1; }});
  }
  BlockingConcurrentQueue<Reducer> reducers(1);
  StoppablePool stp(1);

  // with this initial marking, all but transition e are possible.
  NetMarking m0 = {{"Pa", 4}};
  Model m(net, store, {}, m0);
  for (size_t i = 0; m.getMarking().size() > 0; i++) {
    auto fireable_transitions = m.getFireableTransitions();
    // expected tokens left
    int token_count_before = m.getMarking().size();
    // run only transition b
    m.runTransition("b", reducers, stp);
    // one token should be gone.
    REQUIRE(token_count_before - m.getMarking().size() - 1 == 0);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(22));
  stp.stop();
  // validate we only ran transition b
  REQUIRE(hitmap["a"] == 0);
  REQUIRE(hitmap["b"] == 4);
  REQUIRE(hitmap["c"] == 0);
  REQUIRE(hitmap["d"] == 0);
  REQUIRE(hitmap["e"] == 0);
}
