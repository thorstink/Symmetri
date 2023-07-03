#pragma once
#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>

/**
 * @brief example of a class that has success/fail options and pause/resume
 * functionalities. The custom copy constructor is because atomic<bool> is not
 * default copyable.
 *
 */
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

  bool run() const {
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
    return cancel_.load();
  }
  void cancel() const {
    spdlog::info("cancel {0}!", name);
    cancel_.store(true);
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
