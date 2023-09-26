#include <catch2/catch_test_macros.hpp>
#include <condition_variable>
#include <mutex>

#include "symmetri/symmetri.h"

using namespace symmetri;

std::atomic<bool> can_continue(false), done(false);
void tAllowExitInput() {
  can_continue.store(true);
  while (!done.load()) {
  }  // wait till trigger is done, otherwise we would deadlock
}

TEST_CASE("Test external input.") {
  {
    Net net = {{"t0", {{}, {"Pb"}}},
               {"t1", {{"Pa"}, {"Pb"}}},
               {"t2", {{"Pb", "Pb"}, {"Pc"}}}};
    // we can omit t0, it will be auto-filled as {"t0", DirectMutation{}}
    Store store = {{"t1", &tAllowExitInput}, {"t2", DirectMutation{}}};
    Marking m0 = {{"Pa", PLACEHOLDER_STRING}, {"Pa", PLACEHOLDER_STRING}};
    Marking final_m = {{"Pc", PLACEHOLDER_STRING}};
    auto stp = std::make_shared<TaskSystem>(3);

    PetriNet app(net, m0, final_m, store, {}, "test_net_ext_input", stp);

    // enqueue a trigger;
    stp->push([trigger = app.registerTransitionCallback("t0")]() {
      // sleep a bit so it gets triggered _after_ the net started. Otherwise the
      // net would deadlock
      while (!can_continue.load()) {
      }
      trigger();
      done.store(true);
    });

    // run the net
    auto res = fire(app);
    CHECK(can_continue);
    CHECK(res == state::Completed);
  }
}
