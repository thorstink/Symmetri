#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <list>

#include "petri.h"
#include "symmetri/utilities.hpp"

using namespace symmetri;
using namespace moodycamel;

void t() {}

TEST_CASE(
    "Run a transition with a higher priority over one with a lower priority") {
  std::list<PriorityTable> priorities = {{{"t0", 1}, {"t1", 0}},
                                         {{"t0", 0}, {"t1", 1}}};
  for (auto priority : priorities) {
    Net net = {{"t0", {{"Pa"}, {"Pb"}}}, {"t1", {{"Pa"}, {"Pc"}}}};
    Store store = {{"t0", [] {}}, {"t1", [] {}}};

    Marking m0 = {{"Pa", 1}, {"Pb", 0}, {"Pc", 0}};
    auto stp = std::make_shared<TaskSystem>(1);

    auto m = Petri(net, store, priority, m0, {}, "s", stp);
    m.fireTransitions();
    Reducer r;

    while (
        m.reducer_queue->wait_dequeue_timed(r, std::chrono::milliseconds(1))) {
      r(m);
    }

    auto prio_t0 = std::find_if(priority.begin(), priority.end(), [](auto e) {
                     return e.first == "t0";
                   })->second;
    auto prio_t1 = std::find_if(priority.begin(), priority.end(), [](auto e) {
                     return e.first == "t1";
                   })->second;
    if (prio_t0 > prio_t1) {
      CHECK(MarkingEquality(m.getMarking(), {"Pb"}));
    } else {
      CHECK(MarkingEquality(m.getMarking(), {"Pc"}));
    }
  }
}

TEST_CASE("Using DirectMutation does not queue reducers.") {
  std::list<PriorityTable> priorities = {{{"t0", 1}, {"t1", 0}},
                                         {{"t0", 0}, {"t1", 1}}};
  for (auto priority : priorities) {
    Net net = {{"t0", {{"Pa"}, {"Pb"}}}, {"t1", {{"Pa"}, {"Pc"}}}};
    Store store = {{"t0", DirectMutation{}}, {"t1", DirectMutation{}}};

    Marking m0 = {{"Pa", 1}, {"Pb", 0}, {"Pc", 0}};
    auto stp = std::make_shared<TaskSystem>(1);

    auto m = Petri(net, store, priority, m0, {}, "s", stp);
    m.fireTransitions();
    // no reducers needed, as simple transitions are handled within run all.
    auto prio_t0 = std::find_if(priority.begin(), priority.end(), [](auto e) {
                     return e.first == "t0";
                   })->second;
    auto prio_t1 = std::find_if(priority.begin(), priority.end(), [](auto e) {
                     return e.first == "t1";
                   })->second;
    if (prio_t0 > prio_t1) {
      CHECK(MarkingEquality(m.getMarking(), {"Pb"}));
    } else {
      CHECK(MarkingEquality(m.getMarking(), {"Pc"}));
    }
  }
}
