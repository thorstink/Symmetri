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

std::tuple<Net, Store, PriorityTable, Marking> BugsTestNet() {
  Net net = {{"t", {{"Pa"}, {"Pb"}}}};
  Store store = {{"t", &t}};
  PriorityTable priority;
  Marking m0 = {{"Pa", PLACEHOLDER_STRING}, {"Pa", PLACEHOLDER_STRING}};
  return {net, store, priority, m0};
}

TEST_CASE("Firing the same transition before it can complete should work") {
  auto [net, store, priority, m0] = BugsTestNet();
  auto stp = std::make_shared<TaskSystem>(2);
  Petri m(net, store, priority, m0, {}, "s", stp);
  REQUIRE(m.scheduled_callbacks.empty());
  m.fireTransitions();
  REQUIRE(m.getMarking().empty());
  REQUIRE(m.scheduled_callbacks.size() == 2);

  Reducer r;
  while (
      m.reducer_queue->wait_dequeue_timed(r, std::chrono::milliseconds(250))) {
    r(m);
  }

  {
    std::lock_guard<std::mutex> lk(cv_m);
    is_ready1 = true;
  }

  cv.notify_one();

  m.reducer_queue->wait_dequeue_timed(r, std::chrono::milliseconds(250));
  r(m);
  {
    Marking expected = {{"Pb", PLACEHOLDER_STRING}};
    REQUIRE(MarkingEquality(m.getMarking(), expected));
  }

  // offending test, but fixed :-)
  REQUIRE(m.scheduled_callbacks.size() == 1);
  {
    std::lock_guard<std::mutex> lk(cv_m);
    is_ready2 = true;
  }
  cv.notify_one();
  m.reducer_queue->wait_dequeue_timed(r, std::chrono::milliseconds(250));
  r(m);
  {
    Marking expected = {{"Pb", PLACEHOLDER_STRING}, {"Pb", PLACEHOLDER_STRING}};
    REQUIRE(MarkingEquality(m.getMarking(), expected));
  }

  CHECK(m.scheduled_callbacks.empty());
}
