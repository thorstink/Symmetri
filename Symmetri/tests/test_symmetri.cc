#include <catch2/catch_test_macros.hpp>
#include <filesystem>

#include "Symmetri/symmetri.h"

using namespace symmetri;

void t0() {}
auto t1() {}

std::tuple<StateNet, Store, NetMarking> testNet() {
  StateNet net = {{"t0", {{"Pa", "Pb"}, {"Pc"}}},
                  {"t1", {{"Pc", "Pc"}, {"Pb", "Pb", "Pd"}}}};

  Store store = {{"t0", &t0}, {"t1", &t1}};

  NetMarking m0 = {{"Pa", 4}, {"Pb", 2}, {"Pc", 0}, {"Pd", 0}};
  return {net, store, m0};
}

TEST_CASE("Create a using the net constructor without end condition.") {
  auto [net, store, m0] = testNet();
  symmetri::Application app(net, m0, std::nullopt, store, 3,
                            "test_net_without_end");
  // we can run the net
  auto [ev, res] = app();
  // because there's not final marking, but the net is finite, it deadlocks.
  REQUIRE(res == TransitionState::Deadlock);
  REQUIRE(!ev.empty());
}

TEST_CASE("Create a using the net constructor with end condition.") {
  NetMarking final_marking({{"Pa", 0}, {"Pb", 2}, {"Pc", 0}, {"Pd", 2}});
  auto [net, store, m0] = testNet();
  symmetri::Application app(net, m0, final_marking, store, 3,
                            "test_net_with_end");
  // we can run the net
  auto [ev, res] = app();
  // now there is an end conition.
  REQUIRE(res == TransitionState::Completed);
  REQUIRE(!ev.empty());
}

TEST_CASE("Create a using pnml constructor.") {
  const std::string pnml_file = std::filesystem::current_path().append(
      "../../../Symmetri/tests/assets/PT1.pnml");

  {
    // This store is not appropriate for this net,
    Store store = {{"wrong_id", &t0}};
    symmetri::Application app({pnml_file}, std::nullopt, store, 3, "fail");
    // however, we can try running it,
    auto [ev, res] = app();
    // but the result is an error.
    REQUIRE(res == TransitionState::Error);
    REQUIRE(ev.empty());
  }

  {
    // This store is appropriate for this net,
    Store store = symmetri::Store{{"T0", &t0}};

    NetMarking final_marking({{"P1", 0}});
    symmetri::Application app({pnml_file}, final_marking, store, 3, "success");
    // so we can run it,
    auto [ev, res] = app();
    // and the result is properly completed.
    REQUIRE(res == TransitionState::Completed);
    REQUIRE(!ev.empty());
  }
}
