#pragma once

#include <spdlog/spdlog.h>

#include <random>

#include "symmetri/types.h"

void printLog(const symmetri::Eventlog &eventlog) {
  for (const auto &[caseid, t, s, c] : eventlog) {
    spdlog::info("EventLog: {0}, {1}, {2}, {3}", caseid, t,
                 symmetri::Color::toString(s), c.time_since_epoch().count());
  }
}

symmetri::Marking getGoal(const symmetri::Marking &initial_marking) {
  auto goal = initial_marking;
  for (auto &[p, c] : goal) {
    p = (p == "TaskBucket") ? "SuccessfulTasks" : p;
    c = symmetri::Color::Success;
  }
  return goal;
}

const static symmetri::Token Red(symmetri::Color::registerToken("Red"));

struct Foo {
  Foo(double success_rate, std::chrono::milliseconds sleep_time)
      : sleep_time(sleep_time),
        generator(std::random_device{}()),
        distribution(success_rate) {}
  const std::chrono::milliseconds sleep_time;
  mutable std::default_random_engine generator;
  mutable std::bernoulli_distribution distribution;
};

symmetri::Token fire(const Foo &f) {
  auto now = symmetri::Clock::now();
  while (symmetri::Clock::now() - now < f.sleep_time) {
  }
  return f.distribution(f.generator) ? symmetri::Color::Success : Red;
}

bool isSynchronous(const Foo &) { return false; }
