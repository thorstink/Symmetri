#include <catch2/catch_test_macros.hpp>

#include "petri.h"
#include "symmetri/utilities.hpp"

using namespace symmetri;

TEST_CASE("can fire") {
  // place 1 with color 1
  SmallVectorInput pre_conditions = {{1, 1}};
  std::vector<AugmentedToken> can_fire_marking = {{1, 1}};
  CHECK(canFire(pre_conditions, can_fire_marking));

  // wrong token type
  std::vector<AugmentedToken> can_not_fire_marking = {{1, 0}};
  CHECK(!canFire(pre_conditions, can_not_fire_marking));

  // no tokens type
  CHECK(!canFire(pre_conditions, {}));
}

TEST_CASE("transitions with no inputs can not fire") {
  CHECK(!canFire({}, {}));
}
