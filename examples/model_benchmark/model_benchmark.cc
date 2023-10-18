#include <spdlog/spdlog.h>

#include "symmetri/parsers.h"
#include "symmetri/symmetri.h"
#include "symmetri/utilities.hpp"

int main(int, char *argv[]) {
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%f] [%^%l%$] [thread %t] %v");

  auto pnml1 = std::string(argv[1]);
  const auto &[net, m0] = symmetri::readPnml({pnml1});
  bool bolus = false;
  auto pool = std::make_shared<symmetri::TaskSystem>();

  symmetri::Store store;
  for (const auto &[t, io] : net) {
    if (bolus) {
      store.insert({t, []() {}});
    } else {
      store.insert({t, DirectMutation{}});
    }
  }

  PetriNet bignet(net, m0, {}, store, {}, "pluto", pool);
  spdlog::info("start!");
  const auto start_time = symmetri::Clock::now();
  auto result = fire(bignet);  // infinite loop
  auto el = getLog(bignet);
  const auto end_time = symmetri::Clock::now();
  auto trans_count = el.size() / 2;
  auto delta_t = (double((end_time - start_time).count()) / 1e9);
  auto time_per_trans = delta_t / double(trans_count);
  auto trans_per_second = 1.0 / time_per_trans;
  spdlog::info(
      "finished\ntransitions fired: {0} [], delta t: {3} [s], average time per "
      "transition: "
      "{1} [ns], transitions per second: {2} [t/s]",
      trans_count, time_per_trans * 1e9, trans_per_second, delta_t);
  spdlog::info("Token of this net: {0}", symmetri::Color::toString(result));
  return result == symmetri::Color::Success ? 0 : -1;
}
