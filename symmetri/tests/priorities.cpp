#include <algorithm>
#include <list>

#include "doctest/doctest.h"
#include "petri.h"
#include "symmetri/utilities.hpp"

using namespace symmetri;
using namespace moodycamel;

TEST_CASE(
    "Run a transition with a higher priority over one with a lower priority") {
  std::list<PriorityTable> priorities = {{{"t0", 1}, {"t1", 0}},
                                         {{"t0", 0}, {"t1", 1}}};
  for (auto priority : priorities) {
    Net net = {{"t0", {{{"Pa", Success}}, {{"Pb", Success}}}},
               {"t1", {{{"Pa", Success}}, {{"Pc", Success}}}}};
    Marking m0 = {{"Pa", Success}};
    auto threadpool = std::make_shared<TaskSystem>(1);

    auto m = Petri(net, priority, m0, {}, "s", threadpool);
    m.net.registerCallback("t0", [] {});
    m.net.registerCallback("t1", [] {});
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
      Marking expected = {{"Pb", Success}};
      CHECK(MarkingEquality(m.getMarking(), expected));
    } else {
      Marking expected = {{"Pc", Success}};
      CHECK(MarkingEquality(m.getMarking(), expected));
    }
  }
}

TEST_CASE("Using DirectMutation does not queue reducers.") {
  std::list<PriorityTable> priorities = {{{"t0", 1}, {"t1", 0}},
                                         {{"t0", 0}, {"t1", 1}}};
  for (auto priority : priorities) {
    Net net = {{"t0", {{{"Pa", Success}}, {{"Pb", Success}}}},
               {"t1", {{{"Pa", Success}}, {{"Pc", Success}}}}};

    Marking m0 = {{"Pa", Success}};
    auto threadpool = std::make_shared<TaskSystem>(1);

    auto m = Petri(net, priority, m0, {}, "s", threadpool);
    m.fireTransitions();
    // no reducers needed, as simple transitions are handled within run all.
    auto prio_t0 = std::find_if(priority.begin(), priority.end(), [](auto e) {
                     return e.first == "t0";
                   })->second;
    auto prio_t1 = std::find_if(priority.begin(), priority.end(), [](auto e) {
                     return e.first == "t1";
                   })->second;
    if (prio_t0 > prio_t1) {
      Marking expected = {{"Pb", Success}};
      CHECK(MarkingEquality(m.getMarking(), expected));
    } else {
      Marking expected = {{"Pc", Success}};
      CHECK(MarkingEquality(m.getMarking(), expected));
    }
  }
}
