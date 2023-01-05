#include <catch2/catch_test_macros.hpp>
#include <condition_variable>

#include "symmetri/symmetri.h"

using namespace symmetri;

std::condition_variable cv;
std::mutex cv_m;
bool i_ran(false);
void t() {
  {
    std::lock_guard<std::mutex> lk(cv_m);
    i_ran = true;
  }
  cv.notify_all();
}

TEST_CASE("Test external input.") {
  StateNet net = {{"t0", {{}, {"Pa"}}}};
  Store store = {{"t0", &t}};
  NetMarking m0 = {{"Pa", 0}};
  NetMarking final_m = {{"Pa", 1}};
  StoppablePool stp(3);

  symmetri::Application app(net, m0, final_m, store, {}, "test_net_ext_input",
                            stp);

  // enqueue a test;
  stp.enqueue([]() {
    std::unique_lock<std::mutex> lk(cv_m);
    cv.wait_for(lk, std::chrono::milliseconds(1000), [] { return i_ran; });
  });

  // enqueue a trigger;
  stp.enqueue(
      [trigger = app.registerTransitionCallback("t0")]() { trigger(); });

  // run the net
  auto [ev, res] = app();
  stp.stop();

  REQUIRE(res == TransitionState::Completed);
  REQUIRE(i_ran);
}
