#include <spdlog/spdlog.h>

#include <optional>
#include <random>

#include "Symmetri/symmetri.h"

void helloWorld() { std::this_thread::sleep_for(std::chrono::seconds(1)); }

symmetri::TransitionState failFunc() {
  std::this_thread::sleep_for(std::chrono::seconds(1));
  const double chance = 0.3;  // odds of failing
  std::random_device rd;
  std::mt19937 mt(rd());
  std::bernoulli_distribution dist(chance);
  return dist(mt) ? symmetri::TransitionState::Error
                  : symmetri::TransitionState::Completed;
}

int main(int argc, char *argv[]) {
  auto pnml_path_start = std::string(argv[1]);
  std::string case_id = "alt";

  auto store =
      symmetri::Store{{"t0", &helloWorld},
                      {"t1", &helloWorld},
                      {"t2", symmetri::retryFunc(&failFunc, "t2", case_id)},
                      {"t3", std::nullopt},
                      {"t4", &helloWorld}};

  symmetri::Application net({pnml_path_start}, std::nullopt, store, 2, case_id,
                            true);

  auto [el, result] = net();  // infinite loop

  for (const auto &[caseid, t, s, c, tid] : el) {
    spdlog::info("{0}, {1}, {2}, {3}", caseid, t, printState(s),
                 c.time_since_epoch().count());
  }

  return result == symmetri::TransitionState::Completed ? 0 : -1;
}
