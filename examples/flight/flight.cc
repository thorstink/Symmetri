#include <spdlog/spdlog.h>

#include <iostream>

#include "Symmetri/symmetri.h"
#include "namespace_share_data.h"

std::function<void()> helloT(std::string s) {
  return [s] {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    spdlog::info(s);
    return;
  };
};

int main(int argc, char *argv[]) {
  auto pnml1 = std::string(argv[1]);
  auto pnml2 = std::string(argv[2]);
  auto pnml3 = std::string(argv[3]);

  // symmetri::Application subnet({pnml1}, {{"T0", &action5}}, "charon", false);

  // symmetri::TransitionActionMap store = {
  //     {"T0", subnet},
  //     {"T1", &T1::action1},
  //     // {"T2", &T1::action0}}; // an eventlog function.
  //     // {"T2", &failsSometimes}}; // this is a sometimes failing function.
  //     {"T2", &T1::action1}};

  symmetri::TransitionActionMap store = {
      {"T0", helloT("T0")}, {"T1", helloT("T1")}, {"T2", helloT("T2")}};

  symmetri::Application bignet({pnml1, pnml2, pnml3}, store, "pluto", true);

  auto el = bignet();  // infinite loop

  spdlog::info("Printing trace:\n");
  for (const auto &[caseid, t, s, c] : el) {
    spdlog::info("{0}, {1}, {2}, {3}", caseid, t, printState(s),
                 c.time_since_epoch().count());
  }
}
