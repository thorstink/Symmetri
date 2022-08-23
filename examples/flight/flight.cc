#include <spdlog/spdlog.h>

#include <immer/flex_vector_transient.hpp>
#include <iostream>
#include <random>
#include <thread>

#include "Symmetri/retry.h"
#include "Symmetri/symmetri.h"
#include "Symmetri/ws_interface.hpp"
#include "namespace_share_data.h"

symmetri::Eventlog getNewEvents(const symmetri::Eventlog &el,
                                symmetri::clock_s::time_point t) {
  symmetri::Eventlog new_events;
  auto ne = new_events.transient();
  std::copy_if(el.begin(), el.end(), std::back_inserter(ne),
               [t](const auto &l) { return l.stamp > t; });
  return ne.persistent();
}

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

  symmetri::NetMarking final_marking2 = {{"P2", 1}};
  symmetri::Store s2 = {{"T0", helloT("T01")}, {"T1", helloT("T02")}};
  auto snet = {pnml1, pnml2};

  symmetri::Application subnet(snet, final_marking2, s2, 1, "charon");

  // symmetri::Store store = {{"T0", helloT("T0")},
  symmetri::Store store = {{"T0", subnet},
                           // symmetri::retryFunc(subnet, "T0", "pluto")},
                           {"T1", helloT("T1")},
                           {"T2", helloT("T2")}};
  symmetri::NetMarking final_marking = {{"P3", 30}};
  auto net = {pnml1, pnml2, pnml3};
  symmetri::Application bignet(net, final_marking, store, 3, "pluto");
  auto t = std::thread([&bignet] {
    char a;
    while (true) {
      std::cin >> a;
      bignet.togglePause();
    }
  });

  bool running = true;
  // a server to send stuff (runs a background thread)
  WsServer server(2222);
  // some thread to poll the net and send it away through a server
  auto wt = std::thread([&bignet, &running, &server] {
    auto previous_stamp = symmetri::clock_s::now();
    do {
      auto [t, el, state_net, marking, at] = bignet.get();
      server.sendNet(t, state_net, marking, at);
      auto new_events = getNewEvents(el, previous_stamp);
      if (!new_events.empty()) {
        server.sendLog(new_events);
        previous_stamp = t;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    } while (running);
  });

  WsServer server2(3333);
  // some thread to poll the net and send it away through a server
  auto wt2 = std::thread([&subnet, &running, &server2] {
    auto previous_stamp = symmetri::clock_s::now();
    do {
      auto [t, el, state_net, marking, at] = subnet.get();
      server2.sendNet(t, state_net, marking, at);
      auto new_events = getNewEvents(el, previous_stamp);
      if (!new_events.empty()) {
        server2.sendLog(new_events);
        previous_stamp = t;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    } while (running);
  });

  auto [el, result] = bignet();  // infinite loop

  running = false;
  server.stop();
  server2.stop();
  wt.join();
  wt2.join();

  for (const auto &[caseid, t, s, c, tid] : el) {
    spdlog::info("{0}, {1}, {2}, {3}", caseid, t, printState(s),
                 c.time_since_epoch().count());
  }

  spdlog::info("Result of this net: {0}", printState(result));
  t.detach();

  return result == symmetri::TransitionState::Completed ? 0 : -1;
}
