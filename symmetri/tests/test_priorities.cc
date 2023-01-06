#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>

#include "model.h"
using namespace symmetri;
using namespace moodycamel;

// two transitions
void t(){};

TEST_CASE(
    "Run a transition with a higher priority over one with a lower priority") {
  std::list<std::vector<std::pair<symmetri::Transition, uint8_t>>> priorities =
      {{{"t0", 1}, {"t1", 0}}, {{"t0", 0}, {"t1", 1}}};
  for (auto priority : priorities) {
    BlockingConcurrentQueue<Reducer> reducers(4);
    StateNet net = {{"t0", {{"Pa"}, {"Pb"}}}, {"t1", {{"Pa"}, {"Pc"}}}};
    Store store = {{"t0", std::nullopt}, {"t1", std::nullopt}};

    NetMarking m0 = {{"Pa", 1}, {"Pb", 0}, {"Pc", 0}};
    StoppablePool stp(1);

    auto m = Model(net, store, priority, m0);
    m = runAll(m, reducers, stp);
    Reducer r;

    REQUIRE(reducers.wait_dequeue_timed(r, std::chrono::seconds(1)));
    m = r(std::move(m));

    auto prio_t0 = std::find_if(priority.begin(), priority.end(), [](auto e) {
                     return e.first == "t0";
                   })->second;
    auto prio_t1 = std::find_if(priority.begin(), priority.end(), [](auto e) {
                     return e.first == "t1";
                   })->second;
    if (prio_t0 > prio_t1) {
      CHECK(MarkingEquality(m.tokens, {"Pb"}));
    } else {
      CHECK(MarkingEquality(m.tokens, {"Pc"}));
    }
  }
}
