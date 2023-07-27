#include <catch2/catch_test_macros.hpp>
#include <filesystem>

#include "symmetri/symmetri.h"

using namespace symmetri;

void t0() {}
auto t1() {}

std::tuple<Net, Store, PriorityTable, Marking> testNet() {
  Net net = {{"t0", {{"Pa", "Pb"}, {"Pc"}}},
             {"t1", {{"Pc", "Pc"}, {"Pb", "Pb", "Pd"}}}};

  Store store = {{"t0", &t0}, {"t1", &t1}};
  PriorityTable priority;

  Marking m0 = {{"Pa", 4}, {"Pb", 2}, {"Pc", 0}, {"Pd", 0}};
  return {net, store, priority, m0};
}

TEST_CASE("Create a using the net constructor without end condition.") {
  auto [net, store, priority, m0] = testNet();
  auto stp = std::make_shared<TaskSystem>(1);

  symmetri::PetriNet app(net, m0, {}, store, priority, "test_net_without_end",
                         stp);
  // we can run the net
  auto [ev, res] = fire(app);

  // because there's no final marking, but the net is finite, it deadlocks.
  REQUIRE(res == State::Deadlock);
  REQUIRE(!ev.empty());
}

TEST_CASE("Create a using the net constructor with end condition.") {
  auto stp = std::make_shared<TaskSystem>(1);

  Marking final_marking({{"Pa", 0}, {"Pb", 2}, {"Pc", 0}, {"Pd", 2}});
  auto [net, store, priority, m0] = testNet();
  symmetri::PetriNet app(net, m0, final_marking, store, priority,
                         "test_net_with_end", stp);
  // we can run the net
  auto [ev, res] = fire(app);

  // now there is an end condition.
  REQUIRE(res == State::Completed);
  REQUIRE(!ev.empty());
}

TEST_CASE("Create a using pnml constructor.") {
  const std::string pnml_file = std::filesystem::current_path().append(
      "../../../symmetri/tests/assets/PT1.pnml");

  {
    auto stp = std::make_shared<TaskSystem>(1);
    // This store is not appropriate for this net,
    Store store = {{"wrong_id", &t0}};
    PriorityTable priority;
    symmetri::PetriNet app({pnml_file}, {}, store, priority, "fail", stp);
    // however, we can try running it,
    auto [ev, res] = fire(app);

    // but the result is an error.
    REQUIRE(res == State::Error);
    REQUIRE(ev.empty());
  }

  {
    auto stp = std::make_shared<TaskSystem>(1);
    // This store is appropriate for this net,
    Store store = symmetri::Store{{"T0", &t0}};
    PriorityTable priority;
    Marking final_marking({{"P1", 1}});
    symmetri::PetriNet app({pnml_file}, final_marking, store, priority,
                           "success", stp);
    // so we can run it,
    auto [ev, res] = fire(app);
    // and the result is properly completed.
    REQUIRE(res == State::Completed);
    REQUIRE(!ev.empty());
  }
}

TEST_CASE("Reuse an application with a new case_id.") {
  auto [net, store, priority, m0] = testNet();
  const auto initial_id = "initial0";
  const auto new_id = "something_different0";
  auto stp = std::make_shared<TaskSystem>(1);
  symmetri::PetriNet app(net, m0, {}, store, priority, initial_id, stp);
  REQUIRE(!app.reuseApplication(initial_id));
  REQUIRE(app.reuseApplication(new_id));
  // fire a transition so that there's an entry in the eventlog
  auto eventlog = fire(app).first;
  // double check that the eventlog is not empty
  REQUIRE(!eventlog.empty());
  // the eventlog should have a different case id.
  for (const auto& event : eventlog) {
    REQUIRE(event.case_id == new_id);
  }
}

TEST_CASE("Can not reuse an active application with a new case_id.") {
  auto [net, store, priority, m0] = testNet();
  const auto initial_id = "initial1";
  const auto new_id = "something_different1";
  auto stp = std::make_shared<TaskSystem>(1);
  symmetri::PetriNet app(net, m0, {}, store, priority, initial_id, stp);
  stp->push([&]() mutable {
    // this should fail because we can not do this while everything is active.
    REQUIRE(!app.reuseApplication(new_id));
  });
  auto [ev, res] = fire(app);
}

TEST_CASE("Test pause and resume") {
  std::atomic<int> i = 0;
  Net net = {{"t0", {{"Pa"}, {"Pa"}}}, {"t1", {{}, {"Pb"}}}};
  Store store = {{"t0", [&] { i++; }}, {"t1", [] {}}};
  Marking m0 = {{"Pa", 1}};
  auto stp = std::make_shared<TaskSystem>(2);
  symmetri::PetriNet app(net, m0, {{"Pb", 1}}, store, {}, "random_id", stp);
  int check1, check2;
  stp->push([&, app, t1 = app.registerTransitionCallback("t1")]() {
    const auto dt = std::chrono::milliseconds(5);
    std::this_thread::sleep_for(dt);
    pause(app);
    std::this_thread::sleep_for(dt);
    check1 = i.load();
    std::this_thread::sleep_for(dt);
    check2 = i.load();
    resume(app);
    std::this_thread::sleep_for(dt);
    t1();
  });
  fire(app);
  REQUIRE(check1 == check2);
  REQUIRE(i.load() > check2);
  REQUIRE(i.load() > check2 + 1);
}
