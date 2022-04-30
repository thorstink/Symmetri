#pragma once
#include <blockingconcurrentqueue.h>

#include <thread>
#include <vector>

#include "Symmetri/symmetri.h"

namespace symmetri {
struct StoppablePool {
 private:
  void join() {
    for (auto &&t : pool) {
      t.join();
    }
  }
  std::vector<std::thread> pool;
  std::atomic<bool> stop_flag;
  moodycamel::BlockingConcurrentQueue<object_t> &actions;

  void loop() const {
    object_t transition;
    while (stop_flag.load() == false) {
      if (actions.wait_dequeue_timed(transition,
                                     std::chrono::milliseconds(250))) {
        if (stop_flag.load() == true) {
          break;
        }
        run(transition);
        transition = object_t();
      }
    };
  }

 public:
  StoppablePool(unsigned int thread_count,
                moodycamel::BlockingConcurrentQueue<object_t> &_actions)
      : pool(thread_count), stop_flag(false), actions(_actions) {
    std::generate(std::begin(pool), std::end(pool),
                  [this] { return std::thread(&StoppablePool::loop, this); });
  }
  void stop() {
    stop_flag.store(true);
    for (size_t i = 0; i < pool.size(); ++i) {
      actions.enqueue([] {});
    }
    join();
  }
};

}  // namespace symmetri
