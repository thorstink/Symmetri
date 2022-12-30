#include <spdlog/spdlog.h>

#include <catch2/catch_test_macros.hpp>
#include <condition_variable>

#include "model.h"

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
      spdlog::info("case 1 ({0},{1})", is_ready1, is_ready2);
      return true;
    } else if ((is_ready1 && is_ready2)) {
      spdlog::info("case 2 ({0},{1})", is_ready1, is_ready2);
      return true;
    } else
      return false;
  });
}

std::tuple<StateNet, Store, NetMarking> testNet() {
  StateNet net = {{"t", {{"Pa"}, {"Pb"}}}};
  Store store = {{"t", &t}};
  NetMarking m0 = {{"Pa", 2}};
  return {net, store, m0};
}

TEST_CASE("Firing the same transition before it can complete should work") {
  auto [net, store, m0] = testNet();
  auto m = Model(net, store, m0);
  moodycamel::BlockingConcurrentQueue<Reducer> reducers(4);
  StoppablePool stp(2);
  REQUIRE(m.pending_transitions.empty());
  m = runAll(m, reducers, stp);
  REQUIRE(m.M["Pa"] == 0);
  REQUIRE(m.pending_transitions == std::vector<symmetri::Transition>{"t", "t"});
  REQUIRE(m.active_transition_count == 2);

  {
    std::lock_guard<std::mutex> lk(cv_m);
    is_ready1 = true;
  }
  cv.notify_one();
  Reducer r;

  reducers.wait_dequeue_timed(r, std::chrono::milliseconds(250));
  m = r(std::move(m));
  REQUIRE(m.M["Pb"] == 1);
  // offending test, but fixed :-)
  REQUIRE(m.pending_transitions == std::vector<symmetri::Transition>{"t"});
  REQUIRE(m.active_transition_count == 1);
  {
    std::lock_guard<std::mutex> lk(cv_m);
    is_ready2 = true;
  }
  cv.notify_one();
  reducers.wait_dequeue_timed(r, std::chrono::milliseconds(250));
  m = r(std::move(m));

  REQUIRE(m.M["Pb"] == 2);
  REQUIRE(m.pending_transitions.empty());
}
