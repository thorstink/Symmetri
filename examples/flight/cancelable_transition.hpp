#pragma once
#include <spdlog/spdlog.h>

#include <chrono>
#include <condition_variable>
#include <memory>

#include "symmetri/types.h"

struct Foo {
  Foo() {}
  std::condition_variable cv;
  std::mutex cv_m;
  bool cancel_ = false;
  bool stopped_ = false;
  void run() {
    {
      std::unique_lock<std::mutex> lk(cv_m);
      cancel_ = false;
      stopped_ = false;
      spdlog::info("Running foo");
      cv.wait_for(lk, std::chrono::seconds(3), [this] { return cancel_; });
    }
    if (cancel_) {
      {
        std::lock_guard lk(cv_m);
        stopped_ = true;
      }
      cv.notify_one();
    }
  }
  symmetri::Result cancel() {
    {
      std::lock_guard lk(cv_m);
      cancel_ = true;
    }
    cv.notify_one();
    {
      std::unique_lock<std::mutex> lk(cv_m);
      cv.wait(lk, [this] { return stopped_; });
    }
    spdlog::info("Foo was cancelled");

    return {{}, symmetri::State::UserExit};
  }
};

symmetri::Result fireTransition(const std::shared_ptr<Foo> &f) {
  f->run();
  return {{}, {symmetri::State::Completed}};
}

symmetri::Result cancelTransition(const std::shared_ptr<Foo> &f) {
  return f->cancel();
}

bool isDirectTransition(const std::shared_ptr<Foo> &) { return false; }
