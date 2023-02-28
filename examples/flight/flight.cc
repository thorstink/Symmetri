#include <spdlog/spdlog.h>

#include <csignal>
#include <iostream>
#include <random>
#include <thread>

#include "cancelable_transition.hpp"
#include "symmetri/symmetri.h"

std::function<void()> stop = [] {};
void signal_handler(int) { stop(); }

std::function<void()> helloT(std::string s) {
  return [s] {
    spdlog::info(s);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return;
  };
};

int main(int, char *argv[]) {
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%f] [%^%l%$] [thread %t] %v");
  std::signal(SIGINT, signal_handler);

  auto pnml1 = std::string(argv[1]);
  auto pnml2 = std::string(argv[2]);
  auto pnml3 = std::string(argv[3]);

  auto pool = symmetri::createStoppablePool(4);

  symmetri::Marking final_marking2 = {{"P2", 1}};
  symmetri::Store s2 = {{"T0", helloT("T01")}, {"T1", helloT("T02")}};

  auto snet = {pnml1, pnml2};
  symmetri::Application subnet(snet, final_marking2, s2, {}, "charon", pool);
  auto foo = std::make_shared<Foo>();

  symmetri::Store store = {{"T0", subnet}, {"T1", helloT("T1")}, {"T2", foo}};

  symmetri::Marking final_marking = {{"P3", 5}};
  auto net = {pnml1, pnml2, pnml3};
  std::vector<std::pair<symmetri::Transition, int8_t>> priority;
  symmetri::Application bignet(net, final_marking, store, priority, "pluto",
                               pool);

  // here we re-register the interupt signal so it cleanly exits the net.
  stop = [&] { cancelTransition(bignet); };

  auto [el, result] = fireTransition(bignet);  // infinite loop

  auto el2 = bignet.getEventLog();

  spdlog::info("Result of this net: {0}", printState(result));

  for (const auto &[caseid, t, s, c] : el) {
    spdlog::info("EventLog: {0}, {1}, {2}, {3}", caseid, t, printState(s),
                 c.time_since_epoch().count());
  }

  return result == symmetri::State::Completed ? 0 : -1;
}
