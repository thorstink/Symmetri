#include <catch2/catch_test_macros.hpp>
#include <condition_variable>

#include "petri.h"
#include "symmetri/utilities.hpp"

using namespace symmetri;

std::condition_variable cv;
std::mutex cv_m;
bool is_ready1(false);
bool is_ready2(false);

// this is a function that you can hang using bools
void t() {
  std::unique_lock<std::mutex> lk(cv_m);
  cv.wait(lk, [] {
    if ((is_ready1 && !is_ready2)) {
      return true;
    } else if ((is_ready1 && is_ready2)) {
      return true;
    } else
      return false;
  });
}

std::tuple<Net, Store, PriorityTable, Marking> testNet() {
  Net net = {{"t", {{"Pa"}, {"Pb"}}}};
  Store store = {{"t", &t}};
  PriorityTable priority;
  Marking m0 = {{"Pa", 2}};
  return {net, store, priority, m0};
}

TEST_CASE("Firing the same transition before it can complete should work") {
  auto [net, store, priority, m0] = testNet();
  auto stp = std::make_shared<TaskSystem>(2);
  Petri m(net, store, priority, m0, {}, "s", stp);
  auto reducers =
      std::make_shared<moodycamel::BlockingConcurrentQueue<Reducer>>(4);
  REQUIRE(m.active_transitions.empty());
  m.fireTransitions(reducers, true);
  REQUIRE(m.getMarking().empty());
  CHECK(m.getActiveTransitions() ==
        std::vector<symmetri::Transition>{"t", "t"});
  REQUIRE(m.active_transitions.size() == 2);

  Reducer r;
  while (reducers->wait_dequeue_timed(r, std::chrono::milliseconds(250))) {
    r(m);
  }

  {
    std::lock_guard<std::mutex> lk(cv_m);
    is_ready1 = true;
  }

  cv.notify_one();

  reducers->wait_dequeue_timed(r, std::chrono::milliseconds(250));
  r(m);
  REQUIRE(MarkingEquality(m.getMarking(), {"Pb"}));

  // offending test, but fixed :-)
  CHECK(m.getActiveTransitions() == std::vector<symmetri::Transition>{"t"});
  REQUIRE(m.active_transitions.size() == 1);
  {
    std::lock_guard<std::mutex> lk(cv_m);
    is_ready2 = true;
  }
  cv.notify_one();
  reducers->wait_dequeue_timed(r, std::chrono::milliseconds(250));
  r(m);

  REQUIRE(MarkingEquality(m.getMarking(), {"Pb", "Pb"}));

  CHECK(m.active_transitions.empty());
}
