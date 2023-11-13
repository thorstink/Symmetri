#pragma once

/** @file tasks.h */

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

namespace symmetri {

/**
 * @brief forward declaration of the internal TaskQueue
 *
 */
class TaskQueue;

/**
 * @brief Create a TaskSystem object. The only way to create a TaskSystem
 * is through this factory. We enforce the use of a smart pointer to make sure
 * the pool stays alive until both the original scope of the pool and net is
 * exited.
 *
 * @param thread_count the amount of threads in the pool. The simplest way to
 * avoid deadlocks in the net is to make sure thread_count is at least the
 * maximum amount of transitions in the net. are ran in parallel.
 * @return std::shared_ptr<const TaskSystem>
 */
class TaskSystem {
 public:
  using Task = std::function<void()>;

  /**
   * @brief Construct a new Task System object with n threads
   *
   * @param n_threads if less then 1, it defaults to
   * std::thread::hardware_concurrency()
   */
  explicit TaskSystem(size_t n_threads = std::thread::hardware_concurrency());
  ~TaskSystem() noexcept;
  TaskSystem(TaskSystem const&) = delete;
  TaskSystem(TaskSystem&&) noexcept = delete;
  TaskSystem& operator=(TaskSystem const&) = delete;
  TaskSystem& operator=(TaskSystem&&) noexcept = delete;

  /**
   * @brief push tasks the queue for later execution on the thread pool.
   *
   * @param p
   */
  void push(Task&& p) const;

 private:
  void loop();
  std::vector<std::thread> pool_;
  std::atomic<bool> is_running_;
  std::unique_ptr<TaskQueue> queue_;
};

}  // namespace symmetri
