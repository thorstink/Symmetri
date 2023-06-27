#include <spdlog/spdlog.h>

#include <iostream>

#include "symmetri/symmetri.h"
#include "transition.hpp"

void printLog(const symmetri::Eventlog &el) {
  for (const auto &[caseid, t, s, c] : el) {
    spdlog::info("EventLog: {0}, {1}, {2}, {3}", caseid, t, printState(s),
                 c.time_since_epoch().count());
  }
}

int main(int, char *argv[]) {
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%f] [%^%l%$] [thread %t] %v");

  const auto pnml1 = std::string(argv[1]);
  const auto pnml2 = std::string(argv[2]);
  const auto pnml3 = std::string(argv[3]);

  auto pool = std::make_shared<symmetri::TaskSystem>(4);

  symmetri::PetriNet subnet({pnml1, pnml2}, {{"P2", 1}},
                            {{"T0", Foo("SubFoo")}, {"T1", Foo("SubBar")}}, {},
                            "SubNet", pool);

  symmetri::PetriNet bignet(
      {pnml1, pnml2, pnml3}, {{"P3", 5}},
      {{"T0", subnet}, {"T1", Foo("Bar")}, {"T2", Foo("Foo")}}, {}, "RootNet",
      pool);

  auto t = std::thread([bignet] {
    bool is_running = true;
    while (is_running) {
      std::cout << "input options: \n [p] - pause\n [r] - resume\n [x] - "
                   "exit\n [l] - print log\n";
      char input;
      std::cin >> input;
      switch (input) {
        case 'p':
          pause(bignet);
          break;
        case 'r':
          resume(bignet);
          break;
        case 'l': {
          printLog(bignet.getEventLog());
          break;
        }
        case 'x': {
          cancel(bignet);
          is_running = false;
          break;
        }
        default:
          break;
      }
      std::cin.get();
    };
  });

  auto [el, result] = fire(bignet);
  t.join();

  spdlog::info("Result of this net: {0}", printState(result));

  printLog(el);

  return result == symmetri::State::Completed ? 0 : -1;
}
