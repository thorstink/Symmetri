#include <spdlog/spdlog.h>

#include <csignal>

#include "cancelable_transition.hpp"
#include "symmetri/symmetri.h"

std::function<void()> stop = [] {};
void signal_handler(int) { stop(); }

int main(int, char *argv[]) {
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%f] [%^%l%$] [thread %t] %v");
  std::signal(SIGINT, signal_handler);

  auto pnml1 = std::string(argv[1]);
  auto pnml2 = std::string(argv[2]);
  auto pnml3 = std::string(argv[3]);

  auto pool = symmetri::createStoppablePool(4);

  symmetri::Marking final_marking2 = {{"P2", 1}};
  symmetri::Store s2 = {{"T0", std::make_shared<Foo>("SubFoo")},
                        {"T1", std::make_shared<Foo>("SubBar")}};

  auto snet = {pnml1, pnml2};
  symmetri::Application subnet(snet, final_marking2, s2, {}, "SubNet", pool);

  symmetri::Store store = {{"T0", subnet},
                           {"T1", std::make_shared<Foo>("Bar")},
                           {"T2", std::make_shared<Foo>("Foo")}};

  symmetri::Marking final_marking = {{"P3", 5}};
  auto net = {pnml1, pnml2, pnml3};
  symmetri::PriorityTable priority;
  symmetri::Application bignet(net, final_marking, store, priority, "RootNet",
                               pool);

  // here we re-register the interupt signal so it cleanly exits the net.
  stop = [&] {
    spdlog::info("signal cancel");
    cancelTransition(bignet);
  };

  auto [el, result] = fireTransition(bignet);  // infinite loop

  auto el2 = bignet.getEventLog();
  spdlog::info("Result of this net: {0}", printState(result));

  for (const auto &[caseid, t, s, c] : el) {
    spdlog::info("EventLog: {0}, {1}, {2}, {3}", caseid, t, printState(s),
                 c.time_since_epoch().count());
  }

  return result == symmetri::State::Completed ? 0 : -1;
}
