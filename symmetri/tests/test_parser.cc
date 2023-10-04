#include <catch2/catch_test_macros.hpp>
#include <filesystem>

#include "symmetri/parsers.h"
#include "symmetri/utilities.hpp"

using namespace symmetri;

TEST_CASE("Load p1.pnml net") {
  const std::string pnml_file = std::filesystem::current_path().append(
      "../../../symmetri/tests/assets/n1.pnml");
  const auto &[net, m0] = readPnml({pnml_file});

  // for this particular net, the initial marking is:
  Marking m_expected = {{"P0", TokenLookup::Completed},
                        {"P1", TokenLookup::Completed},
                        {"P2", TokenLookup::Completed},
                        {"P3", TokenLookup::Completed}};
  REQUIRE(m0 == m_expected);

  // for this particular net, the following transitions connect places:
  Net net_test;
  net_test["t0"] = {
      {{"P0", TokenLookup::Completed}, {"P3", TokenLookup::Completed}},
      {{"P4", TokenLookup::Completed},
       {"P4", TokenLookup::Completed},
       {"P7", TokenLookup::Completed}}};
  net_test["t1"] = {{{"P1", TokenLookup::Completed},
                     {"P4", TokenLookup::Completed},
                     {"P4", TokenLookup::Completed}},
                    {{"P5", TokenLookup::Completed}}};
  net_test["t2"] = {
      {{"P2", TokenLookup::Completed}, {"P5", TokenLookup::Completed}},
      {{"P6", TokenLookup::Completed}}};
  net_test["t3"] = {
      {{"P6", TokenLookup::Completed}, {"P8", TokenLookup::Completed}},
      {{"P0", TokenLookup::Completed},
       {"P1", TokenLookup::Completed},
       {"P2", TokenLookup::Completed},
       {"P3", TokenLookup::Completed}}};
  net_test["t4"] = {{{"P7", TokenLookup::Completed}},
                    {{"P8", TokenLookup::Completed}}};

  REQUIRE(stateNetEquality(net_test, net));
}

TEST_CASE("Load p1.grml net") {
  const std::string grml_file = std::filesystem::current_path().append(
      "../../../symmetri/tests/assets/n1.grml");
  const auto &[net, m0, priorities] = readGrml({grml_file});

  // for this particular net, the initial marking is:
  Marking m_expected = {{"P0", TokenLookup::Completed},
                        {"P1", TokenLookup::Completed},
                        {"P2", TokenLookup::Completed},
                        {"P3", TokenLookup::Completed}};
  REQUIRE(m0 == m_expected);

  // for this particular net, the following transitions connect places:
  Net net_test;
  net_test["t0"] = {
      {{"P0", TokenLookup::Completed}, {"P3", TokenLookup::Completed}},
      {{"P4", TokenLookup::Completed},
       {"P4", TokenLookup::Completed},
       {"P7", TokenLookup::Completed}}};
  net_test["t1"] = {{{"P1", TokenLookup::Completed},
                     {"P4", TokenLookup::Completed},
                     {"P4", TokenLookup::Completed}},
                    {{"P5", TokenLookup::Completed}}};
  net_test["t2"] = {
      {{"P2", TokenLookup::Completed}, {"P5", TokenLookup::Completed}},
      {{"P6", TokenLookup::Completed}}};
  net_test["t3"] = {
      {{"P6", TokenLookup::Completed}, {"P8", TokenLookup::Completed}},
      {{"P0", TokenLookup::Completed},
       {"P1", TokenLookup::Completed},
       {"P2", TokenLookup::Completed},
       {"P3", TokenLookup::Completed}}};
  net_test["t4"] = {{{"P7", TokenLookup::Completed}},
                    {{"P8", TokenLookup::Completed}}};

  // for this particular net the priorities are like this:
  PriorityTable priorities_gr = {{"t0", 2}};

  REQUIRE(priorities_gr == priorities);

  REQUIRE(stateNetEquality(net_test, net));
}

TEST_CASE("Load p1_multi.pnml net") {
  const std::string pnml_file = std::filesystem::current_path().append(
      "../../../symmetri/tests/assets/n1_multi.pnml");

  const auto &[net, m0] = readPnml({pnml_file});

  // for this particular net, the initial marking is:
  Marking m_expected = {{"P1", TokenLookup::Completed},
                        {"P2", TokenLookup::Completed},
                        {"P3", TokenLookup::Completed}};
  REQUIRE(m0 == m_expected);

  // for this particular net, the following transitions connect places:
  Net net_test;
  net_test["t0"] = {
      {{"P0", TokenLookup::Completed}, {"P3", TokenLookup::Completed}},
      {{"P4", TokenLookup::Completed}, {"P7", TokenLookup::Completed}}};
  net_test["t1"] = {
      {{"P1", TokenLookup::Completed}, {"P4", TokenLookup::Completed}},
      {{"P5", TokenLookup::Completed}}};
  net_test["t2"] = {
      {{"P2", TokenLookup::Completed}, {"P5", TokenLookup::Completed}},
      {{"P6", TokenLookup::Completed}}};
  // in this variant of this net, the transition from P6 to t3 has weight 2.
  net_test["t3"] = {{{"P6", TokenLookup::Completed},
                     {"P6", TokenLookup::Completed},
                     {"P8", TokenLookup::Completed}},
                    {{"P0", TokenLookup::Completed},
                     {"P1", TokenLookup::Completed},
                     {"P2", TokenLookup::Completed},
                     {"P3", TokenLookup::Completed}}};
  net_test["t4"] = {{{"P7", TokenLookup::Completed}},
                    {{"P8", TokenLookup::Completed}}};
  REQUIRE(stateNetEquality(net_test, net));
}

TEST_CASE("Compose PT1.pnml and PT2.pnml nets") {
  const std::string p1 = std::filesystem::current_path().append(
      "../../../symmetri/tests/assets/PT1.pnml");

  const std::string p2 = std::filesystem::current_path().append(
      "../../../symmetri/tests/assets/PT2.pnml");

  const auto &[net, m0] = readPnml({p1, p2});

  // for this compositions of nets, the initial marking is:
  Marking m_expected = {{"P0", TokenLookup::Completed}};
  REQUIRE(m0 == m_expected);
  // the compositions of these two nets, the following transitions connect
  // places:
  Net net_test;
  net_test["T0"] = {{{"P0", TokenLookup::Completed}},
                    {{"P1", TokenLookup::Completed}}};
  net_test["T1"] = {{{"P1", TokenLookup::Completed}},
                    {{"P2", TokenLookup::Completed}}};
  REQUIRE(stateNetEquality(net_test, net));

  // REQUIRE(net_test == net);
}
