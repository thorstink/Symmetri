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

TEST_CASE("Create a using the net constructor.") {
  auto [net, store, m0] = testNet();
  symmetri::Application app(net, m0, store, 3, "test_net", false);
  // we can run the net
  auto [ev, res] = app();
  REQUIRE(res == TransitionState::Completed);
  REQUIRE(!ev.empty());
}

TEST_CASE("Create a using pnml constructor.") {
  const std::string pnml_file =
      std::filesystem::current_path().append("assets/PT1.pnml");

  {
    // This store is not appropriate for this net,
    Store store = {{"wrong_id", &t0}};
    symmetri::Application app({pnml_file}, store, 3, "fail", false);
    // however, we can try running it,
    auto [ev, res] = app();
    // but the result is an error.
    REQUIRE(res == TransitionState::Error);
    REQUIRE(ev.empty());
  }

  {
    // This store is appropriate for this net,
    Store store = symmetri::Store{{"T0", &t0}};
    symmetri::Application app({pnml_file}, store, 3, "success", false);
    // so we can run it,
    auto [ev, res] = app();
    // and the result is properly completed.
    REQUIRE(res == TransitionState::Completed);
    REQUIRE(!ev.empty());
  }
}
