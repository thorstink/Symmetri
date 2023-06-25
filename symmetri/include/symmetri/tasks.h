#pragma once

#include <atomic>
#include <functional>
#include <thread>
#include <vector>

/// The implementation is based on Sean Parent's task system, described in,
/// Better Code: Concurrency.
namespace symmetri {

class TaskQueue;

/**
 * @brief The task system has a task queue for each thread. It is suggested to
 * have one TaskSystem per executable and share it along the classes that use
 * it. It uses a simply work-stealing mechanism for performance improvement.
 *
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
  explicit TaskSystem(size_t n_threads = 0);
  ~TaskSystem() noexcept;
  TaskSystem(TaskSystem const&) = delete;
  TaskSystem(TaskSystem&&) noexcept = delete;
  TaskSystem& operator=(TaskSystem const&) = delete;
  TaskSystem& operator=(TaskSystem&&) noexcept = delete;
  /**
   * @brief Push allows tasks to be put in one of the TaskQueues
   *
   * @param task
   */
  void push(Task&& task);

 private:
  std::vector<std::thread> threads_;
  std::vector<TaskQueue> queues_;
  std::atomic_size_t counter_;

  /**
   * @brief Run is the loop that runs in each thread. It has the logic to clear
   * a TaskQueue.
   *
   * @param thread_index
   */
  void run_(size_t thread_index);
};

}  // namespace symmetri
