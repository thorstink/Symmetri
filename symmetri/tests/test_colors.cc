#include <catch2/catch_test_macros.hpp>
#include <iostream>

#include "petri.h"
#include "symmetri/utilities.hpp"
using namespace symmetri;

const static Token Blue(registerToken("Blue"));
const static Token Red(registerToken("Red"));
Token red() { return Red; }

TEST_CASE("Test color.") {
  Net net = {{{"t0"}, {{{"p1", "Blue"}}, {{"p2", "Red"}}}}};
  Store store = {{"t0", &red}};
  PriorityTable priority;
  Marking m0 = {{"p1", "Blue"}};
  Marking mgoal = {{"p1", "Red"}};
  auto stp = std::make_shared<TaskSystem>(1);
  Petri m(net, store, priority, m0, mgoal, "s", stp);

  for (auto [p, c] : m.getMarking()) {
    std::cout << p << ", " << c << std::endl;
  }
  // t0 is enabled.
  m.fireTransitions();
  Reducer r1;
  REQUIRE(m.reducer_queue->wait_dequeue_timed(r1, std::chrono::seconds(1)));
  r1(m);
  REQUIRE(m.reducer_queue->wait_dequeue_timed(r1, std::chrono::seconds(1)));
  r1(m);

  const auto marking = m.getMarking();
  // processed but post are not:
  {
    Marking expected = {{"p2", "Red"}};
    for (auto [p, c] : marking) {
      std::cout << p << ", " << c << std::endl;
    }
    REQUIRE(MarkingEquality(marking, expected));
  }
}
