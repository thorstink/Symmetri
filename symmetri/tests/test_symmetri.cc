#include <catch2/catch_test_macros.hpp>
#include <filesystem>

#include "symmetri/symmetri.h"

using namespace symmetri;

void t0() {}
auto t1() {}

std::tuple<StateNet, Store,
           std::vector<std::pair<symmetri::Transition, int8_t>>, NetMarking>
testNet() {
  StateNet net = {{"t0", {{"Pa", "Pb"}, {"Pc"}}},
                  {"t1", {{"Pc", "Pc"}, {"Pb", "Pb", "Pd"}}}};

  Store store = {{"t0", &t0}, {"t1", &t1}};
  std::vector<std::pair<symmetri::Transition, int8_t>> priority;

  NetMarking m0 = {{"Pa", 4}, {"Pb", 2}, {"Pc", 0}, {"Pd", 0}};
  return {net, store, priority, m0};
}

TEST_CASE("Create a using the net constructor without end condition.") {
  auto [net, store, priority, m0] = testNet();
  StoppablePool stp(1);

  symmetri::Application app(net, m0, {}, store, priority,
                            "test_net_without_end", stp);
  // we can run the net
  auto [ev, res] = app();
  stp.stop();

  // because there's no final marking, but the net is finite, it deadlocks.
  REQUIRE(res == TransitionState::Deadlock);
  REQUIRE(!ev.empty());
}

TEST_CASE("Create a using the net constructor with end condition.") {
  StoppablePool stp(1);

  NetMarking final_marking({{"Pa", 0}, {"Pb", 2}, {"Pc", 0}, {"Pd", 2}});
  auto [net, store, priority, m0] = testNet();
  symmetri::Application app(net, m0, final_marking, store, priority,
                            "test_net_with_end", stp);
  // we can run the net
  auto [ev, res] = app();
  stp.stop();

  // now there is an end conition.
  REQUIRE(res == TransitionState::Completed);
  REQUIRE(!ev.empty());
}

TEST_CASE("Create a using pnml constructor.") {
  const std::string pnml_file = std::filesystem::current_path().append(
      "../../../symmetri/tests/assets/PT1.pnml");

  {
    StoppablePool stp(1);
    // This store is not appropriate for this net,
    Store store = {{"wrong_id", &t0}};
    std::vector<std::pair<symmetri::Transition, int8_t>> priority;
    symmetri::Application app({pnml_file}, {}, store, priority, "fail", stp);
    // however, we can try running it,
    auto [ev, res] = app();

    // but the result is an error.
    REQUIRE(res == TransitionState::Error);
    REQUIRE(ev.empty());
    stp.stop();
  }

  {
    StoppablePool stp(1);
    // This store is appropriate for this net,
    Store store = symmetri::Store{{"T0", &t0}};
    std::vector<std::pair<symmetri::Transition, int8_t>> priority;
    NetMarking final_marking({{"P1", 1}});
    symmetri::Application app({pnml_file}, final_marking, store, priority,
                              "success", stp);
    // so we can run it,
    auto [ev, res] = app();
    // and the result is properly completed.
    REQUIRE(res == TransitionState::Completed);
    REQUIRE(!ev.empty());
    stp.stop();
  }
}

TEST_CASE("Run transition manually.") {
  auto [net, store, priority, m0] = testNet();
  StoppablePool stp(1);
  symmetri::Application app(net, m0, {}, store, priority, "single_run_net1",
                            stp);

  REQUIRE(app.tryRunTransition("t0"));
  app.exitEarly();
  stp.stop();
}

TEST_CASE("Run transition that does not exist manually.") {
  auto [net, store, priority, m0] = testNet();
  StoppablePool stp(1);
  symmetri::Application app(net, m0, {}, store, priority, "single_run_net2",
                            stp);

  REQUIRE(!app.tryRunTransition("t0dgdsg"));
  app.exitEarly();
  stp.stop();
}

TEST_CASE("Run transition for which the preconditions are not met manually.") {
  auto [net, store, priority, m0] = testNet();
  StoppablePool stp(1);
  symmetri::Application app(net, m0, {}, store, priority, "single_run_net3",
                            stp);

  auto l = app.getFireableTransitions();
  for (auto t : l) {
    REQUIRE(t != "t1");
  }
  REQUIRE(!app.tryRunTransition("t1"));
  app.exitEarly();
  stp.stop();
}
