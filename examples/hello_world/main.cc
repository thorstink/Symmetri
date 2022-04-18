#include <spdlog/spdlog.h>

#include <chrono>
#include <future>
#include <iostream>
#include <thread>

#include "Symmetri/symmetri.h"
void helloWorld() { std::this_thread::sleep_for(std::chrono::seconds(1)); }

inline std::string printState(symmetri::TransitionState s) {
  return s == symmetri::TransitionState::Started     ? "Started"
         : s == symmetri::TransitionState::Completed ? "Completed"
                                                     : "Error";
}

int main(int argc, char *argv[]) {
  auto pnml_path_start = std::string(argv[1]);
  auto pnml_path_passive = std::string(argv[2]);
  auto store = symmetri::TransitionActionMap{
      {"t0", &helloWorld}, {"t1", &helloWorld}, {"t2", &helloWorld},
      {"t3", &helloWorld}, {"t4", &helloWorld}, {"t50", &helloWorld}};

  symmetri::Application net({pnml_path_start, pnml_path_passive}, store);

  auto t = std::async(std::launch::async, [f = net.push<float>("t50")] {
    float a;
    std::cin >> a;
    f(a);
  });

  auto el = net();  // infinite loop

  for (const auto &[caseid, t, s, c] : el) {
    spdlog::info("{0}, {1}, {2}, {3}", caseid, t, printState(s),
                 c.time_since_epoch().count());
  }
}
