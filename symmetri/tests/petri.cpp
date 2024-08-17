#include "petri.h"

#include <iostream>
#include <map>

#include "doctest/doctest.h"
#include "symmetri/utilities.hpp"

using namespace symmetri;
using namespace moodycamel;

// global counters to keep track of how often the transitions are called.
std::atomic<unsigned int> T0_COUNTER, T1_COUNTER;
// two transitions
void petri0() {
  auto inc = T0_COUNTER.load() + 1;
  T0_COUNTER.store(inc);
}
auto petri1() {
  auto inc = T1_COUNTER.load() + 1;
  T1_COUNTER.store(inc);
  return Success;
}

std::tuple<Net, PriorityTable, Marking> PetriTestNet() {
  T0_COUNTER.store(0);
  T1_COUNTER.store(0);

  Net net = {{"t0", {{{"Pa", Success}, {"Pb", Success}}, {{"Pc", Success}}}},
             {"t1",
              {{{"Pc", Success}, {"Pc", Success}},
               {{"Pb", Success}, {"Pb", Success}, {"Pd", Success}}}}};

  PriorityTable priority;

  Marking m0 = {{"Pa", Success}, {"Pa", Success}, {"Pa", Success},
                {"Pa", Success}, {"Pb", Success}, {"Pb", Success}};
  return {net, priority, m0};
}

TEST_CASE("Test equaliy of nets") {
  auto [net, priority, m0] = PetriTestNet();
  auto net2 = net;
  auto net3 = net;
  CHECK(stateNetEquality(net, net2));
  // change net
  net2.insert({"tx", {{}, {}}});
  CHECK(!stateNetEquality(net, net2));

  // same transitions but different places.
  net3.at("t0").second.push_back({"px", Success});
  CHECK(!stateNetEquality(net, net3));
}

TEST_CASE("Run one transition iteration in a petri net") {
  auto [net, priority, m0] = PetriTestNet();

  auto threadpool = std::make_shared<TaskSystem>(1);
  Petri m(net, priority, m0, {}, "s", threadpool);
  m.net.registerCallback("t0", &petri0);
  m.net.registerCallback("t1", &petri1);

  // t0 is enabled.
  m.fireTransitions();
  // t0 is dispatched but it's reducer has not yet run, so pre-conditions are
  // processed but post are not:
  {
    Marking expected = {{"Pa", Success}, {"Pa", Success}};
    CHECK(MarkingEquality(m.getMarking(), expected));
  }
  // now there should be two reducers;
  Reducer r1, r2;
  CHECK(m.reducer_queue->wait_dequeue_timed(r1, std::chrono::seconds(1)));
  CHECK(m.reducer_queue->wait_dequeue_timed(r1, std::chrono::seconds(1)));
  CHECK(m.reducer_queue->wait_dequeue_timed(r2, std::chrono::seconds(1)));
  CHECK(m.reducer_queue->wait_dequeue_timed(r2, std::chrono::seconds(1)));
  // verify that t0 has actually ran twice.
  CHECK(T0_COUNTER.load() == 2);
  // the marking should still be the same.
  {
    Marking expected = {{"Pa", Success}, {"Pa", Success}};
    CHECK(MarkingEquality(m.getMarking(), expected));
  }

  // process the reducers
  r1(m);
  r2(m);
  // and now the post-conditions are processed:
  CHECK(m.scheduled_callbacks.empty());
  {
    Marking expected = {
        {"Pa", Success}, {"Pa", Success}, {"Pc", Success}, {"Pc", Success}};
    CHECK(MarkingEquality(m.getMarking(), expected));
  }
}

TEST_CASE("Run until net dies") {
  using namespace moodycamel;

  auto [net, priority, m0] = PetriTestNet();
  auto threadpool = std::make_shared<TaskSystem>(1);
  Petri m(net, priority, m0, {}, "s", threadpool);
  m.net.registerCallback("t0", &petri0);
  m.net.registerCallback("t1", &petri1);

  Reducer r;
  Callback a([] {});
  // we need to enqueue one 'no-operation' to start the live net.
  m.reducer_queue->enqueue([](Petri&) {});
  do {
    if (m.reducer_queue->try_dequeue(r)) {
      r(m);
      m.fireTransitions();
    }
  } while (m.scheduled_callbacks.size() > 0);

  // For this specific net we expect:
  Marking expected = {
      {"Pb", Success}, {"Pb", Success}, {"Pd", Success}, {"Pd", Success}};
  CHECK(MarkingEquality(m.getMarking(), expected));

  CHECK(T0_COUNTER.load() == 4);
  CHECK(T1_COUNTER.load() == 2);
}

TEST_CASE("Run until net dies with DirectMutations") {
  using namespace moodycamel;

  auto [net, priority, m0] = PetriTestNet();
  // we can pass an empty story, and DirectMutation will be used for the
  // undefined transitions
  auto threadpool = std::make_shared<TaskSystem>(1);
  Petri m(net, priority, m0, {}, "s", threadpool);

  Reducer r;
  // we need to enqueue one 'no-operation' to start the live net.
  m.reducer_queue->enqueue([](Petri&) {});
  do {
    if (m.reducer_queue->try_dequeue(r)) {
      r(m);
      m.fireTransitions();
    }
  } while (m.scheduled_callbacks.size() > 0);
  // For this specific net we expect:
  Marking expected = {
      {"Pb", Success}, {"Pb", Success}, {"Pd", Success}, {"Pd", Success}};

  CHECK(MarkingEquality(m.getMarking(), expected));
}

TEST_CASE("Step through transitions") {
  std::map<std::string, size_t> hitmap;
  {
    Net net = {{"a", {{{"Pa", Success}}, {}}},
               {"b", {{{"Pa", Success}}, {}}},
               {"c", {{{"Pa", Success}}, {}}},
               {"d", {{{"Pa", Success}}, {}}},
               {"e", {{{"Pb", Success}}, {}}}};
    auto threadpool = std::make_shared<TaskSystem>(1);
    // with this initial marking, all but transition e are possible.
    Marking m0 = {
        {"Pa", Success}, {"Pa", Success}, {"Pa", Success}, {"Pa", Success}};
    Petri m(net, {}, m0, {}, "s", threadpool);
    for (const auto& [t, dm] : net) {
      hitmap.insert({t, 0});
      m.net.registerCallback(t, [&, t = t] { hitmap[t] += 1; });
    }

    // auto scheduled_callbacks = m.getActiveTransitions();
    // CHECK(scheduled_callbacks.size() == 4);  // abcd
    m.tryFire("e");
    m.tryFire("b");
    m.tryFire("b");
    m.tryFire("c");
    m.tryFire("b");
    // there are no reducers ran, so this doesn't update.
    CHECK(m.scheduled_callbacks.size() == 4);
    // there should be no markers left.
    CHECK(m.getMarking().size() == 0);
    // there should be nothing left to fire
    // scheduled_callbacks = m.getActiveTransitions();
    // CHECK(scheduled_callbacks.size() == 0);
    int j = 0;
    Reducer r;
    while (j < 2 * 4 &&
           m.reducer_queue->wait_dequeue_timed(r, std::chrono::seconds(1))) {
      j++;
      r(m);
    }
    // reducers update, there should be active transitions left.
    CHECK(m.scheduled_callbacks.size() == 0);
  }

  // validate we only ran transition 3 times b, 1 time c and none of the others.
  CHECK(hitmap.at("a") == 0);
  CHECK(hitmap.at("b") == 3);
  CHECK(hitmap.at("c") == 1);
  CHECK(hitmap.at("d") == 0);
  CHECK(hitmap.at("e") == 0);
}

TEST_CASE("create fireable transitions shortlist") {
  auto [net, priority, m0] = PetriTestNet();
  auto threadpool = std::make_shared<TaskSystem>(1);
  Petri m(net, priority, m0, {}, "s", threadpool);
}
