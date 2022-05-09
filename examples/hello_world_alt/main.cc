#include <spdlog/spdlog.h>

#include "Symmetri/symmetri.h"
void helloWorld() { std::this_thread::sleep_for(std::chrono::seconds(1)); }

int main(int argc, char *argv[]) {
  auto pnml_path_start = std::string(argv[1]);
  auto store = symmetri::Store{{"t0", &helloWorld},
                               {"t1", &helloWorld},
                               {"t2", &helloWorld},
                               {"t3", &helloWorld},
                               {"t4", &helloWorld}};

  symmetri::Application net({pnml_path_start}, store, 2, "l", true);

  auto [el, result] = net();  // infinite loop

  for (const auto &[caseid, t, s, c, tid] : el) {
    spdlog::info("{0}, {1}, {2}, {3}", caseid, t, printState(s),
                 c.time_since_epoch().count());
  }

  return result == symmetri::TransitionState::Completed ? 0 : -1;
}
