#include <spdlog/spdlog.h>

#include <iostream>
#include <optional>
#include <random>

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

void helloWorld() { std::this_thread::sleep_for(std::chrono::seconds(1)); }

symmetri::TransitionState failFunc() {
  std::this_thread::sleep_for(std::chrono::seconds(1));
  const double chance = 0.5;  // odds of failing
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

  symmetri::Application net({pnml_path_start}, std::nullopt, store, 2, case_id);

  bool running = true;
  // a server to send stuff (runs a background thread)
  WsServer server(2222);
  // some thread to poll the net and send it away through a server
  auto t = std::thread([&net, &running, &server] {
    auto previous_stamp = symmetri::clock_s::now();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    do {
      auto [t, el, state_net, marking, at] = net.get();
      server.sendNet(t, state_net, marking, at);
      server.sendLog(getNewEvents(el, previous_stamp));
      previous_stamp = t;
      spdlog::info("log entries: {0}", el.size());
      std::this_thread::sleep_for(std::chrono::seconds(2));
    } while (running);
  });

  auto [el, result] = net();  // infinite loop

  running = false;
  server.stop();
  t.join();
  for (const auto &[caseid, t, s, c, tid] : el) {
    spdlog::info("{0}, {1}, {2}, {3}", caseid, t, printState(s),
                 c.time_since_epoch().count());
  }
  spdlog::info("Result of this net: {0}", printState(result));

  return result == symmetri::TransitionState::Completed ? 0 : -1;
}
