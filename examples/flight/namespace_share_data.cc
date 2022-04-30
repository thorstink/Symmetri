#include "namespace_share_data.h"

#include <cstdlib>
#include <random>
#include <thread>

void sleep(std::chrono::milliseconds ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
  return;
}
namespace T1 {
bool Boo = true;

std::vector<symmetri::Event> action0() noexcept {
  std::vector<symmetri::Event> log;
  log.push_back({"case_id", "action0", symmetri::TransitionState::Started,
                 symmetri::clock_t::now()});

  T1::Boo = true;
  sleep(std::chrono::milliseconds(500 * 5));

  log.push_back({"case_id", "action0", symmetri::TransitionState::Completed,
                 symmetri::clock_t::now()});
  return log;
}

void action1() noexcept {
  T1::Boo = false;
  sleep(std::chrono::milliseconds(700 * 5));
}
}  // namespace T1

std::vector<symmetri::Event> failsSometimes() noexcept {
  std::vector<symmetri::Event> log;
  log.push_back({"case_id", "action2", symmetri::TransitionState::Started,
                 symmetri::clock_t::now()});

  sleep(std::chrono::milliseconds(200 * 5));
  const double chance =
      0.3;  // this is the chance of getting true, between 0 and 1;
  std::random_device rd;
  std::mt19937 mt(rd());
  std::bernoulli_distribution dist(chance);
  bool err = dist(mt);

  if (err) {
    log.push_back({"case_id", "action2", symmetri::TransitionState::Error,
                   symmetri::clock_t::now()});
  } else {
    log.push_back({"case_id", "action2", symmetri::TransitionState::Completed,
                   symmetri::clock_t::now()});
  }
  return log;
}
void action3() noexcept { sleep(std::chrono::milliseconds(600 * 5)); }
void action4() noexcept {
  auto dur = 1800 + 200 * 5 + std::rand() / ((RAND_MAX + 1500u) / 1500);
  sleep(std::chrono::milliseconds(dur));
}

void action5() noexcept { sleep(std::chrono::milliseconds(450 * 5)); }

void action6() noexcept { sleep(std::chrono::milliseconds(250 * 5)); }
