#include <spdlog/spdlog.h>

#include <iostream>
#include <random>
#include <thread>

#include "Symmetri/retry.h"
#include "Symmetri/symmetri.h"
#include "Symmetri/ws_interface.h"

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

int main(int argc, char *argv[]) {
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%f] [%^%l%$] [thread %t] %v");

  auto pnml1 = std::string(argv[1]);
  auto pnml2 = std::string(argv[2]);
  auto pnml3 = std::string(argv[3]);

  symmetri::NetMarking final_marking2 = {{"P2", 1}};
  symmetri::Store s2 = {{"T0", helloT("T01")}, {"T1", helloT("T02")}};
  auto snet = {pnml1, pnml2};

  symmetri::Application subnet(snet, final_marking2, s2, 1, "charon");

  symmetri::Store store = {
      {"T0", subnet}, {"T1", helloT("T1")}, {"T2", helloT("T2")}};

  symmetri::NetMarking final_marking = {{"P3", 5}};
  auto net = {pnml1, pnml2, pnml3};
  symmetri::Application bignet(net, final_marking, store, 3, "pluto");

  std::atomic<bool> running(true);

  // some thread to poll the net and send it away through a server
  auto wt = std::jthread([&bignet, &running] {
    auto server = WsServer(2222, [&]() { bignet.togglePause(); });
    auto previous_stamp = symmetri::clock_s::now();
    do {
      bignet.doMeData([&] {
        auto [t, el, state_net, marking, at] =
            bignet.get();  // not _thread safe_
        auto new_events = getNewEvents(el, previous_stamp);
        if (!new_events.empty()) {
          server.sendNet(t, state_net, marking, at);
          server.sendLog(new_events);
          previous_stamp = t;
        }
      });
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    } while (running.load());
    server.stop();
  });

  auto [el, result] = bignet();  // infinite loop

  running.store(false);

  for (const auto &[caseid, t, s, c, tid] : el) {
    spdlog::info("{0}, {1}, {2}, {3}", caseid, t, printState(s),
                 c.time_since_epoch().count());
  }

  spdlog::info("Result of this net: {0}", printState(result));

  return result == symmetri::TransitionState::Completed ? 0 : -1;
}
