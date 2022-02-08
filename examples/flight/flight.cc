#include <iostream>

#include "Symmetri/symmetri.h"
#include "namespace_share_data.h"

int main(int argc, char *argv[]) {
  auto pnml1 = std::string(argv[1]);
  auto pnml2 = std::string(argv[2]);
  auto pnml3 = std::string(argv[3]);

  auto action = [subnet = symmetri::start({pnml1}, {{"t0", &action5}})]()
      -> symmetri::OptionalError {
    subnet();
    return std::nullopt;
  };

  symmetri::TransitionActionMap store = {
      {"t0", action}, {"t1", &T1::action0}, {"t2", &T1::action1}};

  auto bignet = symmetri::start({pnml1, pnml2, pnml3}, store);

  bignet();
}
