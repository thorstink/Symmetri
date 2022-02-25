#include <iostream>

#include "Symmetri/symmetri.h"
#include "namespace_share_data.h"

int main(int argc, char *argv[]) {
  auto pnml1 = std::string(argv[1]);
  auto pnml2 = std::string(argv[2]);
  auto pnml3 = std::string(argv[3]);

  symmetri::Application subnet({pnml1}, {{"T0", &action5}}, "pluto|charon",
                               false);

  symmetri::TransitionActionMap store = {
      {"T0", subnet}, {"T1", &T1::action0}, {"T2", &T1::action1}};

  symmetri::Application bignet({pnml1, pnml2, pnml3}, store, "pluto", true);

  bignet();
}
