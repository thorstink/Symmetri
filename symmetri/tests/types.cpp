#include "symmetri/types.h"

#include "doctest/doctest.h"
#include "symmetri/utilities.hpp"

using namespace symmetri;

TEST_CASE("The same marking should be equal") {
  std::vector<Place> m1, m2;
  m1 = {"a", "b", "c"};
  m2 = {"a", "b", "c"};

  CHECK(m1 == m2);
  CHECK(MarkingEquality(m1, m2));
  CHECK(MarkingEquality(m2, m1));
}

TEST_CASE("Different marking should not be equal") {
  std::vector<Place> m1, m2;
  m1 = {"a", "b", "c"};
  m2 = {"a", "b", "c", "d"};
  CHECK(m1 != m2);
  CHECK(!MarkingEquality(m1, m2));
  CHECK(!MarkingEquality(m2, m1));
}

TEST_CASE(
    "The same marking should be equal and should be independent of order") {
  std::vector<Place> m1, m2;
  m1 = {"a", "b", "c"};
  m2 = {"a", "c", "b"};

  CHECK(m1 != m2);
  CHECK(MarkingEquality(m1, m2));
  CHECK(MarkingEquality(m2, m1));
}

TEST_CASE(
    "The marking is reached with marking is exactly a subset of final "
    "marking V1") {
  std::vector<Place> marking, final_marking;
  marking = {"a", "b", "c"};
  final_marking = {"a"};
  CHECK(MarkingReached(marking, final_marking));
}

TEST_CASE(
    "The marking is reached with marking is exactly a subset of final "
    "marking V2") {
  std::vector<Place> marking, final_marking;
  marking = {"a", "a", "b", "c"};
  final_marking = {"a", "a"};
  CHECK(MarkingReached(marking, final_marking));
}

TEST_CASE(
    "The marking is not reached when marking is not exactly a subset of final "
    "marking") {
  std::vector<Place> marking, final_marking;
  marking = {"a", "a", "b", "c"};
  final_marking = {"a"};
  CHECK(!MarkingReached(marking, final_marking));
}

TEST_CASE("The marking is reached when it is exactly the final marking") {
  std::vector<Place> marking, final_marking;
  marking = {"a"};
  final_marking = {"a"};
  CHECK(MarkingReached(marking, final_marking));
}

TEST_CASE("The marking is not reached when markings are completely disjoint") {
  std::vector<Place> marking, final_marking;
  marking = {"a"};
  final_marking = {"b"};
  CHECK(!MarkingReached(marking, final_marking));
}

TEST_CASE("The marking is not reached if the final marking is empty") {
  std::vector<Place> marking, final_marking;
  marking = {"a"};
  final_marking = {};
  CHECK(!MarkingReached(marking, final_marking));
}

TEST_CASE(
    "The marking is not reached if the final marking is empty, even if they "
    "are both empty") {
  std::vector<Place> marking, final_marking;
  marking = {};
  final_marking = {};
  CHECK(!MarkingReached(marking, final_marking));
  CHECK(MarkingEquality(marking, final_marking));
}
