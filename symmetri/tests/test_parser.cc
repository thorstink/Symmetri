#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <iostream>

#include "symmetri/pnml_parser.h"

using namespace symmetri;

TEST_CASE("Load p1.pnml net") {
  const std::string pnml_file = std::filesystem::current_path().append(
      "../../../symmetri/tests/assets/n1.pnml");
  const auto &[net, m0] = readPetriNets({pnml_file});

  // for this particular net, the initial marking is:
  Marking m_test;
  m_test["P0"] = 1;
  m_test["P1"] = 1;
  m_test["P2"] = 1;
  m_test["P3"] = 1;
  m_test["P4"] = 0;
  m_test["P5"] = 0;
  m_test["P6"] = 0;
  m_test["P7"] = 0;
  m_test["P8"] = 0;
  REQUIRE(m_test == m0);

  // for this particular net, the following transitions connect places:
  Net net_test;
  net_test["t0"] = {{"P0", "P3"}, {"P4", "P7"}};
  net_test["t1"] = {{"P1", "P4"}, {"P5"}};
  net_test["t2"] = {{"P2", "P5"}, {"P6"}};
  net_test["t3"] = {{"P6", "P8"}, {"P0", "P1", "P2", "P3"}};
  net_test["t4"] = {{"P7"}, {"P8"}};
  REQUIRE(stateNetEquality(net_test, net));
}

TEST_CASE("Load p1_multi.pnml net") {
  const std::string pnml_file = std::filesystem::current_path().append(
      "../../../symmetri/tests/assets/n1_multi.pnml");

  const auto &[net, m0] = readPetriNets({pnml_file});

  // for this particular net, the initial marking is:
  Marking m_test;
  m_test["P0"] = 0;
  m_test["P1"] = 1;
  m_test["P2"] = 1;
  m_test["P3"] = 1;
  m_test["P4"] = 0;
  m_test["P5"] = 0;
  m_test["P6"] = 0;
  m_test["P7"] = 0;
  m_test["P8"] = 0;
  REQUIRE(m_test == m0);

  // for this particular net, the following transitions connect places:
  Net net_test;
  net_test["t0"] = {{"P0", "P3"}, {"P4", "P7"}};
  net_test["t1"] = {{"P1", "P4"}, {"P5"}};
  net_test["t2"] = {{"P2", "P5"}, {"P6"}};
  // in this variant of this net, the transition from P6 to t3 has weight 2.
  net_test["t3"] = {{"P6", "P6", "P8"}, {"P0", "P1", "P2", "P3"}};
  net_test["t4"] = {{"P7"}, {"P8"}};
  REQUIRE(stateNetEquality(net_test, net));
}

TEST_CASE("Compose PT1.pnml and PT2.pnml nets") {
  const std::string p1 = std::filesystem::current_path().append(
      "../../../symmetri/tests/assets/PT1.pnml");

  const std::string p2 = std::filesystem::current_path().append(
      "../../../symmetri/tests/assets/PT2.pnml");

  const auto &[net, m0] = readPetriNets({p1, p2});

  // for this compositions of nets, the initial marking is:
  Marking m_test;
  m_test["P0"] = 1;
  m_test["P1"] = 0;
  m_test["P2"] = 0;
  REQUIRE(m_test == m0);

  // the compositions of these two nets, the following transitions connect
  // places:
  Net net_test;
  net_test["T0"] = {{"P0"}, {"P1"}};
  net_test["T1"] = {{"P1"}, {"P2"}};
  REQUIRE(stateNetEquality(net_test, net));

  // REQUIRE(net_test == net);
}
