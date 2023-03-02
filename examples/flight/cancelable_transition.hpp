#pragma once
#include <spdlog/spdlog.h>

#include <chrono>
#include <memory>

#include "symmetri/types.h"

struct Foo {
  const std::string message;
  const std::chrono::seconds t;
  Foo(std::string m, int s = 2) : message(m), t(s) {}
  std::atomic<bool> cancel_ = false;
  symmetri::State run() {
    cancel_.store(false);
    spdlog::info("Running {0}", message);
    std::this_thread::sleep_for(t);
    return cancel_.load() ? symmetri::State::UserExit
                          : symmetri::State::Completed;
  }
  symmetri::Result cancel() {
    cancel_.store(true);
    spdlog::info("Cancelled {0}", message);
    return {{}, symmetri::State::UserExit};
  }
};

symmetri::Result fireTransition(const std::shared_ptr<Foo> &f) {
  return {{}, f->run()};
}

symmetri::Result cancelTransition(const std::shared_ptr<Foo> &f) {
  return f->cancel();
}

bool isDirectTransition(const std::shared_ptr<Foo> &) { return false; }
