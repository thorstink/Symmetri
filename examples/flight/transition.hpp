#pragma once
#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>

#include "symmetri/polyaction.h"
#include "symmetri/types.h"

class Foo {
 private:
  const std::string name;
  const unsigned count;
  const std::chrono::seconds interval;
  mutable std::atomic<bool> cancel_;
  mutable std::atomic<bool> is_paused_;

 public:
  Foo(std::string m, unsigned count = 5, int s = 1)
      : name(m), count(count), interval(s), cancel_(false), is_paused_(false) {}
  Foo(const Foo &o)
      : name(o.name),
        count(o.count),
        interval(o.interval),
        cancel_(false),
        is_paused_(false) {}

  symmetri::State run() const {
    cancel_.store(false);
    is_paused_.store(false);
    spdlog::info("Running {0}", name);
    unsigned counter = 0;
    do {
      if (!is_paused_.load()) {
        spdlog::info("Running {0} {1}", name, counter++);
      } else {
        spdlog::info("Paused {0} {1}", name, counter);
      }
      std::this_thread::sleep_for(interval);
    } while (counter < count && !cancel_.load());

    return cancel_.load() ? symmetri::State::UserExit
                          : symmetri::State::Completed;
  }
  symmetri::Result cancel() const {
    cancel_.store(true);
    spdlog::info("cancel {0}!", name);
    return {{}, symmetri::State::UserExit};
  }

  void pause() const {
    spdlog::info("pause {0}!", name);
    is_paused_.store(true);
  }
  void resume() const {
    spdlog::info("resume {0}!", name);
    is_paused_.store(false);
  }
};

namespace symmetri {
template <>
Result fire<Foo>(const Foo &f) {
  return {{}, f.run()};
}
template <>
Result cancel<Foo>(const Foo &f) {
  return f.cancel();
}
template <>
bool isDirect<Foo>(const Foo &) {
  return false;
}
template <>
void pause<Foo>(const Foo &f) {
  f.pause();
}
template <>
void resume<Foo>(const Foo &f) {
  f.resume();
}

}  // namespace symmetri
