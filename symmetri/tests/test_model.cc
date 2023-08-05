#include <catch2/catch_test_macros.hpp>
#include <map>

#include "model.h"
#include "symmetri/utilities.hpp"

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
  return symmetri::State::Completed;
}

std::tuple<Net, Store, PriorityTable, Marking> testNet() {
  T0_COUNTER.store(0);
  T1_COUNTER.store(0);

  Net net = {{"t0", {{"Pa", "Pb"}, {"Pc"}}},
             {"t1", {{"Pc", "Pc"}, {"Pb", "Pb", "Pd"}}}};

  Store store = {{"t0", &t0}, {"t1", &t1}};
  PriorityTable priority;

  Marking m0 = {{"Pa", 4}, {"Pb", 2}, {"Pc", 0}, {"Pd", 0}};
  return {net, store, priority, m0};
}

TEST_CASE("Test equaliy of nets") {
  auto [net, store, priority, m0] = testNet();
  auto net2 = net;
  auto net3 = net;
  REQUIRE(stateNetEquality(net, net2));
  // change net
  net2.insert({"tx", {{}, {}}});
  REQUIRE(!stateNetEquality(net, net2));

  // same transitions but different places.
  net3.at("t0").second.push_back("px");
  REQUIRE(!stateNetEquality(net, net3));
}

TEST_CASE("Run one transition iteration in a petri net") {
  auto [net, store, priority, m0] = testNet();

  auto stp = std::make_shared<TaskSystem>(1);
  Petri m(net, store, priority, m0, {}, "s", stp);
  auto reducers = std::make_shared<BlockingConcurrentQueue<Reducer>>(4);

  // t0 is enabled.
  m.fireTransitions(reducers, true, "");
  // t0 is dispatched but it's reducer has not yet run, so pre-conditions are
  // processed but post are not:
  REQUIRE(m.getActiveTransitions() ==
          std::vector<symmetri::Transition>{"t0", "t0"});
  REQUIRE(m.getMarking() == std::vector<symmetri::Place>{"Pa", "Pa"});

  // now there should be two reducers;
  Reducer r1, r2;
  REQUIRE(reducers->wait_dequeue_timed(r1, std::chrono::seconds(1)));
  REQUIRE(reducers->wait_dequeue_timed(r1, std::chrono::seconds(1)));
  REQUIRE(reducers->wait_dequeue_timed(r2, std::chrono::seconds(1)));
  REQUIRE(reducers->wait_dequeue_timed(r2, std::chrono::seconds(1)));
  // verify that t0 has actually ran twice.
  REQUIRE(T0_COUNTER.load() == 2);
  // the marking should still be the same.
  REQUIRE(m.getMarking() == std::vector<symmetri::Place>{"Pa", "Pa"});

  CHECK(m.getActiveTransitions() ==
        std::vector<symmetri::Transition>{"t0", "t0"});

  // process the reducers
  r1(m);
  r2(m);
  // and now the post-conditions are processed:
  REQUIRE(m.active_transitions_n.empty());
  REQUIRE(MarkingEquality(
      m.getMarking(), std::vector<symmetri::Place>{"Pa", "Pa", "Pc", "Pc"}));
}

TEST_CASE("Run until net dies") {
  using namespace moodycamel;

  auto [net, store, priority, m0] = testNet();
  auto stp = std::make_shared<TaskSystem>(1);
  Petri m(net, store, priority, m0, {}, "s", stp);

  auto reducers = std::make_shared<BlockingConcurrentQueue<Reducer>>(4);

  Reducer r;
  PolyTransition a([] {});
  // we need to enqueue one 'no-operation' to start the live net.
  reducers->enqueue([](Petri&) {});
  do {
    if (reducers->try_dequeue(r)) {
      r(m);
      m.fireTransitions(reducers, true);
    }
  } while (m.active_transitions_n.size() > 0);

  // For this specific net we expect:
  REQUIRE(MarkingEquality(
      m.getMarking(), std::vector<symmetri::Place>{"Pb", "Pb", "Pd", "Pd"}));

  REQUIRE(T0_COUNTER.load() == 4);
  REQUIRE(T1_COUNTER.load() == 2);
}

TEST_CASE("Run until net dies with nullptr") {
  using namespace moodycamel;

  auto [net, store, priority, m0] = testNet();
  store = {{"t0", DirectMutation{}}, {"t1", DirectMutation{}}};
  auto stp = std::make_shared<TaskSystem>(1);
  Petri m(net, store, priority, m0, {}, "s", stp);

  auto reducers = std::make_shared<BlockingConcurrentQueue<Reducer>>(4);

  Reducer r;
  PolyTransition a([] {});
  // we need to enqueue one 'no-operation' to start the live net.
  reducers->enqueue([](Petri&) {});
  do {
    if (reducers->try_dequeue(r)) {
      r(m);
      m.fireTransitions(reducers, true);
    }
  } while (m.active_transitions_n.size() > 0);

  // For this specific net we expect:
  REQUIRE(MarkingEquality(
      m.getMarking(), std::vector<symmetri::Place>{"Pb", "Pb", "Pd", "Pd"}));
}

TEST_CASE(
    "getFireableTransitions return a vector of unique of unique fireable "
    "transitions") {
  Net net = {{"a", {{"Pa"}, {}}},
             {"b", {{"Pa"}, {}}},
             {"c", {{"Pa"}, {}}},
             {"d", {{"Pa"}, {}}},
             {"e", {{"Pb"}, {}}}};

  Store store;
  for (auto [t, dm] : net) {
    store.insert({t, DirectMutation{}});
  }
  // with this initial marking, all but transition e are possible.
  Marking m0 = {{"Pa", 1}};
  auto stp = std::make_shared<TaskSystem>(1);

  Petri m(net, store, {}, m0, {}, "s", stp);
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
  std::map<std::string, size_t> hitmap;
  {
    Net net = {{"a", {{"Pa"}, {}}},
               {"b", {{"Pa"}, {}}},
               {"c", {{"Pa"}, {}}},
               {"d", {{"Pa"}, {}}},
               {"e", {{"Pb"}, {}}}};
    Store store;
    for (const auto& [t, dm] : net) {
      hitmap.insert({t, 0});
      store.insert({t, [&, t = t] { hitmap[t] += 1; }});
    }
    auto reducers = std::make_shared<BlockingConcurrentQueue<Reducer>>(5);
    auto stp = std::make_shared<TaskSystem>(1);
    // with this initial marking, all but transition e are possible.
    Marking m0 = {{"Pa", 4}};
    Petri m(net, store, {}, m0, {}, "s", stp);
    REQUIRE(m.getFireableTransitions().size() == 4);  // abcd
    m.fire("e", reducers);
    m.fire("b", reducers);
    m.fire("b", reducers);
    m.fire("c", reducers);
    m.fire("b", reducers);
    // there are no reducers ran, so this doesn't update.
    REQUIRE(m.getActiveTransitions().size() == 4);
    // there should be no markers left.
    REQUIRE(m.getMarking().size() == 0);
    // there should be nothing left to fire
    REQUIRE(m.getFireableTransitions().size() == 0);
    int j = 0;
    Reducer r;
    while (j < 2 * 4 &&
           reducers->wait_dequeue_timed(r, std::chrono::seconds(1))) {
      j++;
      r(m);
    }
    // reducers update, there should be active transitions left.
    REQUIRE(m.getActiveTransitions().size() == 0);
  }

  // validate we only ran transition 3 times b, 1 time c and none of the others.
  REQUIRE(hitmap.at("a") == 0);
  REQUIRE(hitmap.at("b") == 3);
  REQUIRE(hitmap.at("c") == 1);
  REQUIRE(hitmap.at("d") == 0);
  REQUIRE(hitmap.at("e") == 0);
}
