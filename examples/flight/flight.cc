#include <spdlog/spdlog.h>

#include <iostream>
#include <random>
#include <thread>

#include "symmetri/retry.h"
#include "symmetri/symmetri.h"

symmetri::Eventlog getNewEvents(const symmetri::Eventlog &el,
                                symmetri::clock_s::time_point t) {
  symmetri::Eventlog new_events;
  std::copy_if(el.begin(), el.end(), std::back_inserter(new_events),
               [t](const auto &l) { return l.stamp > t; });
  return new_events;
}

std::function<void()> helloT(std::string s) {
  return [s] {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    spdlog::info(s);
    return;
  };
};

int main(int, char *argv[]) {
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%f] [%^%l%$] [thread %t] %v");

  auto pnml1 = std::string(argv[1]);
  auto pnml2 = std::string(argv[2]);
  auto pnml3 = std::string(argv[3]);

  symmetri::StoppablePool pool(4);

  symmetri::NetMarking final_marking2 = {{"P2", 1}};
  symmetri::Store s2 = {{"T0", helloT("T01")}, {"T1", helloT("T02")}};
  auto snet = {pnml1, pnml2};

  symmetri::Application subnet(snet, final_marking2, s2, {}, "charon", pool);

  symmetri::Store store = {
      {"T0", subnet}, {"T1", helloT("T1")}, {"T2", helloT("T2")}};

  symmetri::NetMarking final_marking = {{"P3", 5}};
  auto net = {pnml1, pnml2, pnml3};
  std::vector<std::pair<symmetri::Transition, int8_t>> priority;
  symmetri::Application bignet(net, final_marking, store, priority, "pluto",
                               pool);

  std::atomic<bool> running(true);

  auto [el, result] = bignet();  // infinite loop
  running.store(false);
  for (const auto &[caseid, t, s, c, tid] : el) {
    spdlog::info("{0}, {1}, {2}, {3}", caseid, t, printState(s),
                 c.time_since_epoch().count());
  }

  spdlog::info("Result of this net: {0}", printState(result));

  return result == symmetri::TransitionState::Completed ? 0 : -1;
}
