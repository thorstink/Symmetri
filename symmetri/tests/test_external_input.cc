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
    Net net = {
        {"t0", {{}, {{"Pb", TokenLookup::Completed}}}},
        {"t1",
         {{{"Pa", TokenLookup::Completed}}, {{"Pb", TokenLookup::Completed}}}},
        {"t2",
         {{{"Pb", TokenLookup::Completed}, {"Pb", TokenLookup::Completed}},
          {{"Pc", TokenLookup::Completed}}}}};
    // we can omit t0, it will be auto-filled as {"t0", DirectMutation{}}
    Store store = {{"t1", &tAllowExitInput}, {"t2", DirectMutation{}}};
    Marking m0 = {{"Pa", TokenLookup::Completed},
                  {"Pa", TokenLookup::Completed}};
    Marking final_m = {{"Pc", TokenLookup::Completed}};
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
