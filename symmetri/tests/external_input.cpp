#include <atomic>

#include "doctest/doctest.h"
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
    Net net = {{"t0", {{}, {{"Pb", Success}}}},
               {"t1", {{{"Pa", Success}}, {{"Pb", Success}}}},
               {"t2", {{{"Pb", Success}, {"Pb", Success}}, {{"Pc", Success}}}}};

    Marking initial_marking = {{"Pa", Success}, {"Pa", Success}};
    Marking goal_marking = {{"Pc", Success}};
    auto threadpool = std::make_shared<TaskSystem>(3);

    PetriNet app(net, "test_net_ext_input", threadpool, initial_marking,
                 goal_marking);
    app.registerCallback("t1", &tAllowExitInput);

    // enqueue a trigger;
    threadpool->push([trigger = app.getInputTransitionHandle("t0")]() {
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
    CHECK(res == Success);
  }
}
