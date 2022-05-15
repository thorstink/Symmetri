#include <spdlog/spdlog.h>

#include <random>

#include "Symmetri/symmetri.h"
#include "namespace_share_data.h"

std::function<void()> helloT(std::string s) {
  return [s] {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    spdlog::info(s);
    return;
  };
};

symmetri::TransitionState failFunc() {
  std::this_thread::sleep_for(std::chrono::seconds(1));
  const double chance = 0.3;  // odds of failing
  std::random_device rd;
  std::mt19937 mt(rd());
  std::bernoulli_distribution dist(chance);
  bool fail = dist(mt);
  fail ? spdlog::info("hi fail") : spdlog::info("hi success");
  return fail ? symmetri::TransitionState::Error
              : symmetri::TransitionState::Completed;
}

int main(int argc, char *argv[]) {
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%f] [%^%l%$] [thread %t] %v");

  auto pnml1 = std::string(argv[1]);
  auto pnml2 = std::string(argv[2]);
  auto pnml3 = std::string(argv[3]);

  symmetri::Application subnet({pnml1}, {{"T0", &failFunc}}, 1, "charon",
                               false);

  symmetri::Store store = {{"T0", symmetri::retryFunc(subnet, "T0", "pluto")},
                           {"T1", helloT("T1")},
                           {"T2", helloT("T2")}};

  symmetri::Application bignet({pnml1, pnml2, pnml3}, store, 3, "pluto", true);

  auto [el, result] = bignet();  // infinite loop

  for (const auto &[caseid, t, s, c, tid] : el) {
    spdlog::info("{0}, {1}, {2}, {3}", caseid, t, printState(s),
                 c.time_since_epoch().count());
  }

  return result == symmetri::TransitionState::Completed ? 0 : -1;
}
