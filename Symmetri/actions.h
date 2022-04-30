#pragma once
#include <blockingconcurrentqueue.h>

#include <thread>
#include <vector>

#include "Symmetri/symmetri.h"
#include "model.h"

namespace symmetri {
struct StoppablePool {
  StoppablePool(unsigned int thread_count,
                const std::shared_ptr<std::atomic<bool>> &_stop,
                const std::function<void()> &worker,
                moodycamel::BlockingConcurrentQueue<object_t> &_actions)
      : pool(thread_count), stop_flag(_stop), actions(_actions) {
    std::generate(std::begin(pool), std::end(pool),
                  [worker]() { return std::thread(worker); });
  }
  void stop() {
    stop_flag->store(true);
    for (size_t i = 0; i < pool.size(); ++i) {
      actions.enqueue([] {});
    }
    join();
  }

 private:
  void join() {
    for (auto &&t : pool) {
      t.join();
    }
  }
  std::vector<std::thread> pool;
  std::shared_ptr<std::atomic<bool>> stop_flag;
  moodycamel::BlockingConcurrentQueue<object_t> &actions;
};

StoppablePool executeTransition(
    moodycamel::BlockingConcurrentQueue<object_t> &actions,
    unsigned int thread_count, const std::string &case_id);

}  // namespace symmetri
