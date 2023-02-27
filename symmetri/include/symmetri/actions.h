#pragma once

#include <atomic>
#include <thread>
#include <vector>

#include "symmetri/polyaction.h"

namespace symmetri {

class StoppablePool;

/**
 * @brief Create a StoppablePool object. The only way to create a StoppablePool
 * is through this factory. We enforce the use of a smart pointer to make sure
 * the pool stays alive until both the original scope of the pool and net is
 * exited.
 *
 * @param thread_count the amount of threads in the pool. The simplest way to
 * avoid deadlocks in the net is to make sure thread_count is at least the
 * maximum amount of transitions in the net. are ran in parallel.
 * @return std::shared_ptr<const StoppablePool>
 */
std::shared_ptr<const StoppablePool> createStoppablePool(
    unsigned int thread_count);

/**
 * @brief A StoppablePool is a small threadpool based on a
 * single-producer-multi-consumer thread-safe queue.
 *
 */
class StoppablePool {
 private:
  void loop();
  std::vector<std::thread> pool;
  std::atomic<bool> stop_flag;
  StoppablePool(const StoppablePool&) = delete;
  StoppablePool& operator=(const StoppablePool&) = delete;
  friend std::shared_ptr<const StoppablePool> createStoppablePool(
      unsigned int thread_count) {
    return std::shared_ptr<const StoppablePool>(
        new StoppablePool(thread_count));
  }
  StoppablePool(unsigned int thread_count);

 public:
  ~StoppablePool();

  /**
   * @brief enqueue allows to put almost anything as "payload" for a transition.
   *
   * @param p
   */
  void enqueue(std::function<void()>&& p) const;
};

}  // namespace symmetri
