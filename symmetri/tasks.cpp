#include "symmetri/tasks.h"

#include <cassert>
#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>

namespace symmetri {

// task stealing parameter
constexpr size_t NumRounds = 4;

class TaskQueue {
 public:
  TaskQueue();
  bool pop(TaskSystem::Task& task);
  void push(TaskSystem::Task const& task);
  bool try_pop(TaskSystem::Task& task);
  bool try_push(TaskSystem::Task const& task);
  void done();

 private:
  using lock_type = std::unique_lock<std::mutex>;
  std::deque<TaskSystem::Task> queue_;
  bool done_;
  std::mutex mutex_;
  std::condition_variable ready_;
};

TaskSystem::TaskSystem(size_t n_threads)
    : queues_{n_threads > 0 ? n_threads : std::thread::hardware_concurrency()},
      counter_{0} {
  threads_.reserve(queues_.size());
  for (size_t n = 0; n < queues_.size(); ++n) {
    threads_.emplace_back([this, n] { this->run_(n); });
  }
}

TaskSystem::~TaskSystem() noexcept {
  for (TaskQueue& q : queues_) q.done();
  for (std::thread& t : threads_) t.join();
}

void TaskSystem::push(Task&& task) {
  size_t const count = counter_++;
  if constexpr (NumRounds > 0) {
    size_t const n_max = queues_.size() * NumRounds;
    for (size_t n = 0; n < n_max; ++n) {
      size_t const index = (count + n) % queues_.size();
      if (queues_[index].try_push(std::forward<Task>(task))) return;
    }
  }
  size_t const index = count % queues_.size();
  queues_[index].push(std::forward<Task>(task));
}

void TaskSystem::run_(size_t thread_index) {
  while (true) {
    Task task;
    if constexpr (NumRounds > 0) {
      for (size_t n = 0; n < queues_.size(); ++n) {
        size_t const index = (thread_index + n) % queues_.size();
        if (queues_[index].try_pop(task)) break;
      }
    }
    if (not task and not queues_[thread_index].pop(task)) break;
    assert(task);
    task();
  }
}

TaskQueue::TaskQueue() : done_{false} {}

bool TaskQueue::pop(TaskSystem::Task& task) {
  auto lock = lock_type{mutex_};
  while (queue_.empty() and not done_) {
    ready_.wait(lock);
  }
  if (queue_.empty()) return false;
  task = std::move(queue_.front());
  queue_.pop_front();
  return true;
}

void TaskQueue::push(TaskSystem::Task const& task) {
  {
    auto lock = lock_type{mutex_};
    queue_.push_back(task);
  }
  ready_.notify_one();
}

bool TaskQueue::try_pop(TaskSystem::Task& task) {
  auto lock = lock_type{mutex_, std::try_to_lock};
  if (not lock or queue_.empty()) return false;
  task = std::move(queue_.front());
  queue_.pop_front();
  return true;
}

bool TaskQueue::try_push(TaskSystem::Task const& task) {
  {
    auto lock = lock_type{mutex_, std::try_to_lock};
    if (not lock) return false;
    queue_.push_back(task);
  }
  ready_.notify_one();
  return true;
}

void TaskQueue::done() {
  {
    auto lock = lock_type{mutex_};
    done_ = true;
  }
  ready_.notify_all();
}
}  // namespace symmetri
