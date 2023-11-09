#include <atomic>
#include <catch2/catch_test_macros.hpp>

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
    Net net = {{"t0", {{}, {{"Pb", Color::toString(Color::Success)}}}},
               {"t1",
                {{{"Pa", Color::toString(Color::Success)}},
                 {{"Pb", Color::toString(Color::Success)}}}},
               {"t2",
                {{{"Pb", Color::toString(Color::Success)},
                  {"Pb", Color::toString(Color::Success)}},
                 {{"Pc", Color::toString(Color::Success)}}}}};

    Marking m0 = {{"Pa", Color::toString(Color::Success)},
                  {"Pa", Color::toString(Color::Success)}};
    Marking final_m = {{"Pc", Color::toString(Color::Success)}};
    auto stp = std::make_shared<TaskSystem>(3);

    PetriNet app(net, m0, final_m, {}, "test_net_ext_input", stp);
    app.registerTransitionCallback("t1", &tAllowExitInput);

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
    CHECK(res == Color::Success);
  }
}
