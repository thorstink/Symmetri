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

std::tuple<Net, PriorityTable, Marking> BugsTestNet() {
  Net net = {{"t",
              {{{"Pa", Color::toString(Color::Success)}},
               {{"Pb", Color::toString(Color::Success)}}}}};
  PriorityTable priority;
  Marking m0 = {{"Pa", Color::toString(Color::Success)},
                {"Pa", Color::toString(Color::Success)}};
  return {net, priority, m0};
}

TEST_CASE("Firing the same transition before it can complete should work") {
  auto [net, priority, m0] = BugsTestNet();
  auto stp = std::make_shared<TaskSystem>(2);
  Petri m(net, priority, m0, {}, "s", stp);
  m.net.registerTransitionCallback("t", &t);

  CHECK(m.scheduled_callbacks.empty());
  m.fireTransitions();
  CHECK(m.getMarking().empty());
  CHECK(m.scheduled_callbacks.size() == 2);

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
    Marking expected = {{"Pb", Color::toString(Color::Success)}};
    CHECK(MarkingEquality(m.getMarking(), expected));
  }

  // offending test, but fixed :-)
  CHECK(m.scheduled_callbacks.size() == 1);
  {
    std::lock_guard<std::mutex> lk(cv_m);
    is_ready2 = true;
  }
  cv.notify_one();
  m.reducer_queue->wait_dequeue_timed(r, std::chrono::milliseconds(250));
  r(m);
  {
    Marking expected = {{"Pb", Color::toString(Color::Success)},
                        {"Pb", Color::toString(Color::Success)}};
    CHECK(MarkingEquality(m.getMarking(), expected));
  }

  CHECK(m.scheduled_callbacks.empty());
}
