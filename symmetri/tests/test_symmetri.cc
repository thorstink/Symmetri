#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <iostream>

#include "symmetri/symmetri.h"

using namespace symmetri;

void t0() {}
auto t1() {}

std::tuple<Net, Store, PriorityTable, Marking> SymmetriTestNet() {
  Net net = {{"t0",
              {{{"Pa", Color::toString(Color::Success)},
                {"Pb", Color::toString(Color::Success)}},
               {{"Pc", Color::toString(Color::Success)}}}},
             {"t1",
              {{{"Pc", Color::toString(Color::Success)},
                {"Pc", Color::toString(Color::Success)}},
               {{"Pb", Color::toString(Color::Success)},
                {"Pb", Color::toString(Color::Success)},
                {"Pd", Color::toString(Color::Success)}}}}};

  Store store = {{"t0", &t0}, {"t1", &t1}};
  PriorityTable priority;

  Marking m0 = {{"Pa", Color::toString(Color::Success)},
                {"Pa", Color::toString(Color::Success)},
                {"Pa", Color::toString(Color::Success)},
                {"Pa", Color::toString(Color::Success)},
                {"Pb", Color::toString(Color::Success)},
                {"Pb", Color::toString(Color::Success)}};
  return {net, store, priority, m0};
}

TEST_CASE("Create a using the net constructor without end condition.") {
  auto [net, store, priority, m0] = SymmetriTestNet();
  auto stp = std::make_shared<TaskSystem>(1);

  PetriNet app(net, m0, {}, store, priority, "test_net_without_end", stp);
  // we can run the net
  auto res = fire(app);
  auto ev = getLog(app);
  // because there's no final marking, but the net is finite, it deadlocks.
  CHECK(res == Color::Deadlock);
  CHECK(!ev.empty());
}

TEST_CASE("Create a using the net constructor with end condition.") {
  auto stp = std::make_shared<TaskSystem>(1);

  Marking final_marking({{"Pb", Color::toString(Color::Success)},
                         {"Pb", Color::toString(Color::Success)},
                         {"Pd", Color::toString(Color::Success)},
                         {"Pd", Color::toString(Color::Success)}});

  auto [net, store, priority, m0] = SymmetriTestNet();
  PetriNet app(net, m0, final_marking, store, priority, "test_net_with_end",
               stp);
  // we can run the net
  auto res = fire(app);
  auto ev = getLog(app);

  // now there is an end condition.
  CHECK(res == Color::Success);
  CHECK(!ev.empty());
}

TEST_CASE("Create a using pnml constructor.") {
  const std::string pnml_file = std::filesystem::current_path().append(
      "../../../symmetri/tests/assets/PT1.pnml");

  auto stp = std::make_shared<TaskSystem>(1);
  // This store is appropriate for this net,
  Store store = symmetri::Store{{"T0", &t0}};
  PriorityTable priority;
  Marking final_marking({{"P1", Color::toString(Color::Success)}});
  PetriNet app({pnml_file}, final_marking, store, priority, "success", stp);
  // so we can run it,
  auto res = fire(app);
  auto ev = getLog(app);
  // and the result is properly completed.
  CHECK(res == Color::Success);
  CHECK(!ev.empty());
}

TEST_CASE("Reuse an application with a new case_id.") {
  auto [net, store, priority, m0] = SymmetriTestNet();
  const auto initial_id = "initial0";
  const auto new_id = "something_different0";
  auto stp = std::make_shared<TaskSystem>(1);
  PetriNet app(net, m0, {}, store, priority, initial_id, stp);
  CHECK(!app.reuseApplication(initial_id));
  CHECK(app.reuseApplication(new_id));
  // fire a transition so that there's an entry in the eventlog
  fire(app);
  auto eventlog = getLog(app);
  // double check that the eventlog is not empty
  CHECK(!eventlog.empty());
  // the eventlog should have a different case id.
  for (const auto& event : eventlog) {
    CHECK(event.case_id == new_id);
  }
}

TEST_CASE("Can not reuse an active application with a new case_id.") {
  auto [net, store, priority, m0] = SymmetriTestNet();
  const auto initial_id = "initial1";
  const auto new_id = "something_different1";
  auto stp = std::make_shared<TaskSystem>(1);
  PetriNet app(net, m0, {}, store, priority, initial_id, stp);
  stp->push([&]() mutable {
    // this should fail because we can not do this while everything is active.
    CHECK(!app.reuseApplication(new_id));
  });

  fire(app);
}

TEST_CASE("Test pause and resume") {
  std::atomic<int> i = 0;
  Net net = {{"t0",
              {{{"Pa", Color::toString(Color::Success)}},
               {{"Pa", Color::toString(Color::Success)}}}},
             {"t1", {{}, {{"Pb", Color::toString(Color::Success)}}}}};
  Store store = {{"t0", [&] { i++; }}, {"t1", [] {}}};
  Marking m0 = {{"Pa", Color::toString(Color::Success)}};
  auto stp = std::make_shared<TaskSystem>(2);
  PetriNet app(net, m0, {{"Pb", Color::toString(Color::Success)}}, store, {},
               "random_id", stp);
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
  CHECK(check1 == check2);
  CHECK(i.load() > check2);
  CHECK(i.load() > check2 + 1);
}
namespace symmetri {
namespace state {
const static Token ExternalState(Color::registerToken("ExternalState"));
}
}  // namespace symmetri

TEST_CASE("Types") {
  using namespace symmetri::state;
  CHECK(Color::Scheduled != ExternalState);
  CHECK(Color::Started != ExternalState);
  CHECK(Color::Success != ExternalState);
  CHECK(Color::Deadlock != ExternalState);
  CHECK(Color::Paused != ExternalState);
  CHECK(Color::UserExit != ExternalState);
  CHECK(Color::Error != ExternalState);
  CHECK(ExternalState == ExternalState);
  CHECK(Color::toString(ExternalState) != "");
}

TEST_CASE("Print Types") {
  using namespace symmetri::state;
  std::cout << Color::toString(Color::Scheduled) << ", " << Color::Scheduled
            << std::endl;
  std::cout << Color::toString(Color::Started) << ", " << Color::Started
            << std::endl;
  std::cout << Color::toString(Color::Success) << ", " << Color::Success
            << std::endl;
  std::cout << Color::toString(Color::Deadlock) << ", " << Color::Deadlock
            << std::endl;
  std::cout << Color::toString(Color::Paused) << ", " << Color::Paused
            << std::endl;
  std::cout << Color::toString(Color::UserExit) << ", " << Color::UserExit
            << std::endl;
  std::cout << Color::toString(Color::Error) << ", " << Color::UserExit
            << std::endl;
  std::cout << Color::toString(ExternalState) << ", " << ExternalState
            << std::endl;
}
