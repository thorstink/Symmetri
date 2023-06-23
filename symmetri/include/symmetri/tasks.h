#pragma once

#include <atomic>
#include <functional>
#include <thread>
#include <vector>

/// The implementation is based on Sean Parent's task system, described in,
/// Better Code: Concurrency.
namespace symmetri {

class TaskQueue;

class TaskSystem {
 public:
  using Task = std::function<void()>;
  explicit TaskSystem(size_t n_threads = 0);
  ~TaskSystem() noexcept;
  TaskSystem(TaskSystem const&) = delete;
  TaskSystem(TaskSystem&&) noexcept = delete;
  TaskSystem& operator=(TaskSystem const&) = delete;
  TaskSystem& operator=(TaskSystem&&) noexcept = delete;
  void push(Task&& task);

 private:
  std::vector<std::thread> threads_;
  std::vector<TaskQueue> queues_;
  std::atomic_size_t counter_;

  void run_(size_t thread_index);
};

}  // namespace symmetri
