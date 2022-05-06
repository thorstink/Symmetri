#include <catch2/catch_test_macros.hpp>

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

std::tuple<StateNet, Store, NetMarking> testNet() {
  T0_COUNTER = 0;
  T1_COUNTER = 0;

  StateNet net = {{"t0", {{"Pa", "Pb"}, {"Pc"}}},
                  {"t1", {{"Pc", "Pc"}, {"Pb", "Pb", "Pd"}}}};

  Store store = {{"t0", &t0}, {"t1", &t1}};

  NetMarking m0 = {{"Pa", 4}, {"Pb", 2}, {"Pc", 0}, {"Pd", 0}};
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
  REQUIRE(T0_COUNTER == 1);
}

TEST_CASE("Run one transition iteration in a petri net") {
  using namespace moodycamel;

  auto [net, store, m0] = testNet();
  auto m = Model(net, store, m0);
  BlockingConcurrentQueue<Reducer> reducers(4);
  BlockingConcurrentQueue<PolyAction> polymorphic_actions(4);

  // t0 is enabled.
  m = runAll(m, reducers, polymorphic_actions);
  // t0 is dispatched but not yet run, so pre-conditions are processed but post
  // are not:
  REQUIRE(m.pending_transitions == std::set<Transition>({"t0"}));
  REQUIRE(m.M == NetMarking({{"Pa", 3}, {"Pb", 1}, {"Pc", 0}, {"Pd", 0}}));
  Reducer r;
  // there is no reducer yet because the task hasn't been executed yet.
  REQUIRE(!reducers.try_dequeue(r));
  PolyAction a([] {});
  // there is an action to be dequed.
  REQUIRE(polymorphic_actions.try_dequeue(a));
  // execture the actual task
  runTransition(a);
  // now there should be a reducer
  REQUIRE(T0_COUNTER == 1);
  REQUIRE(reducers.try_dequeue(r));
  // the marking should still be the same.
  REQUIRE(m.pending_transitions == std::set<Transition>({"t0"}));
  REQUIRE(m.M == NetMarking({{"Pa", 3}, {"Pb", 1}, {"Pc", 0}, {"Pd", 0}}));
  // process the reducer
  m = r(std::move(m));
  // and now the post-conditions are processed:
  REQUIRE(m.pending_transitions.empty());
  REQUIRE(m.M == NetMarking({{"Pa", 3}, {"Pb", 1}, {"Pc", 1}, {"Pd", 0}}));
}

TEST_CASE("Run until net dies") {
  using namespace moodycamel;

  auto [net, store, m0] = testNet();
  auto m = Model(net, store, m0);
  BlockingConcurrentQueue<Reducer> reducers(4);
  BlockingConcurrentQueue<PolyAction> polymorphic_actions(4);

  Reducer r;
  PolyAction a([] {});
  // we need to enqueue one 'no-operation' to start the live net.
  reducers.enqueue([](Model&& m) -> Model& { return m; });
  do {
    if (reducers.try_dequeue(r)) {
      m = runAll(r(std::move(m)), reducers, polymorphic_actions);
    }
    if (polymorphic_actions.try_dequeue(a)) {
      runTransition(a);
    }
  } while (!m.pending_transitions.empty());

  // For this specific net we expect:
  REQUIRE(m.M == NetMarking({{"Pa", 0}, {"Pb", 2}, {"Pc", 0}, {"Pd", 2}}));
  REQUIRE(T0_COUNTER == 4);
  REQUIRE(T1_COUNTER == 2);
}

TEST_CASE("Marking memoization") {
  using namespace moodycamel;

  auto [net, store, m0] = testNet();
  BlockingConcurrentQueue<Reducer> reducers(4);
  BlockingConcurrentQueue<PolyAction> polymorphic_actions(4);

  // take two copies.
  auto m_one = Model(net, store, m0);
  REQUIRE(m_one.cache.size() == 0);
  m_one = runAll(m_one, reducers, polymorphic_actions);
  REQUIRE(m_one.cache.size() == 1);
  // for this test, we leave this specific PolyAction queued and not execute it.
  // The counter hence stays 0.
  REQUIRE(T0_COUNTER == 0);

  // the next time we have m0, we expect to be able to retrieve the following
  // from the cache:
  std::tuple<NetMarking, std::vector<PolyAction>, std::set<std::string>>
      expect_memoization = {m_one.M, {PolyAction(&t0)}, {"t0"}};

  const auto actual_memoization = m_one.cache[hashNM(m0)];

  REQUIRE(std::get<NetMarking>(actual_memoization) ==
          std::get<NetMarking>(expect_memoization));

  REQUIRE(std::get<std::set<std::string>>(actual_memoization) ==
          std::get<std::set<std::string>>(expect_memoization));

  // note that we can not easily check equality PolyActions afaik. But to be
  // sure, we will see that both the actual and expected memoized PolyAction
  // increment t0-counter.
  runTransition(std::get<std::vector<PolyAction>>(actual_memoization)[0]);
  REQUIRE(T0_COUNTER == 1);
  runTransition(std::get<std::vector<PolyAction>>(expect_memoization)[0]);
  REQUIRE(T0_COUNTER == 2);
}
