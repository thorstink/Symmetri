#include "namespace_share_data.h"

#include <cstdlib>
#include <random>
#include <thread>

using namespace symmetri;

void sleep(std::chrono::milliseconds ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
  return;
}
namespace T1 {
bool Boo = true;

OptionalError action0() {
  T1::Boo = true;
  sleep(std::chrono::milliseconds(500 * 5));
  return std::nullopt;
}
OptionalError action1() {
  T1::Boo = false;
  sleep(std::chrono::milliseconds(700 * 5));
  return std::nullopt;
}
}  // namespace T1

OptionalError action2() {
  sleep(std::chrono::milliseconds(200 * 5));
  const double chance =
      0.3;  // this is the chance of getting true, between 0 and 1;
  std::random_device rd;
  std::mt19937 mt(rd());
  std::bernoulli_distribution dist(chance);
  bool err = dist(mt);
  if (err) {
    return MarkingMutation{{"p6", 1}};
  }
  return std::nullopt;
}
OptionalError action3() {
  sleep(std::chrono::milliseconds(600 * 5));
  return std::nullopt;
}
OptionalError action4() {
  auto dur = 1800 + 200 * 5 + std::rand() / ((RAND_MAX + 1500u) / 1500);
  sleep(std::chrono::milliseconds(dur));
  return std::nullopt;
}

OptionalError action5() {
  sleep(std::chrono::milliseconds(450 * 5));
  return std::nullopt;
}

OptionalError action6() {
  sleep(std::chrono::milliseconds(250 * 5));
  return std::nullopt;
}
