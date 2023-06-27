#pragma once
#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include "symmetri/types.h"

struct Foo {
  const std::string message;
  const std::chrono::seconds t;
  Foo(std::string m, int s = 1) : message(m), t(s), is_paused_(false) {}
  std::atomic<bool> cancel_;
  std::atomic<bool> is_paused_;
  symmetri::State run() {
    cancel_.store(false);
    is_paused_.store(false);
    spdlog::info("Running {0}", message);
    auto counter = 0;
    do {
      if (!is_paused_.load()) {
        spdlog::info("Running {0} {1}", message, counter++);
      } else {
        spdlog::info("Paused {0} {1}", message, counter);
      }
      std::this_thread::sleep_for(t);
    } while (counter < 5 && !cancel_.load());

    return cancel_.load() ? symmetri::State::UserExit
                          : symmetri::State::Completed;
  }
  symmetri::Result cancel() {
    cancel_.store(true);
    spdlog::info("cancel {0}!", message);
    return {{}, symmetri::State::UserExit};
  }

  void pause() {
    spdlog::info("pause {0}!", message);
    is_paused_.store(true);
  }
  void resume() {
    spdlog::info("resume {0}!", message);
    is_paused_.store(false);
  }
};

namespace symmetri {
Result fire(const std::shared_ptr<Foo> &f) { return {{}, f->run()}; }
Result cancel(const std::shared_ptr<Foo> &f) { return f->cancel(); }
bool isDirect(const std::shared_ptr<Foo> &) { return false; }
void pause(const std::shared_ptr<Foo> &f) { f->pause(); }
void resume(const std::shared_ptr<Foo> &f) { f->resume(); }

}  // namespace symmetri
