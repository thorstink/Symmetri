#include "symmetri/tasks.h"

#include "queue/blockingconcurrentqueue.h"

namespace symmetri {

/**
 * @brief An alias for the implementation. In this case
 * moodycamel::BlockingConcurrentQueue.
 *
 */
using Queue = moodycamel::BlockingConcurrentQueue<TaskSystem::Task>;

/**
 * @brief TaskQueue is an inheritance based alias for a lock-free queue.
 *
 */
class TaskQueue : public Queue {
  using Queue::Queue;
};

TaskSystem::TaskSystem(size_t thread_count)
    : pool_(thread_count),
      is_running_(false),
      queue_(std::make_unique<TaskQueue>(256)) {
  std::generate(std::begin(pool_), std::end(pool_),
                [this] { return std::thread(&TaskSystem::loop, this); });
}

TaskSystem::~TaskSystem() noexcept {
  is_running_.store(true, std::memory_order_release);
  for (size_t i = 0; i < pool_.size(); ++i) {
    queue_->enqueue([] {});
  }
  for (auto &t : pool_) {
    if (t.joinable()) {
      t.join();
    }
  }
}

void TaskSystem::loop() {
  while (true) {
    Task transition;
    if (queue_->wait_dequeue_timed(transition, -1)) {
      if (is_running_.load(std::memory_order_acquire)) {
        break;
      }
      transition();
    }
  }
}

void TaskSystem::push(Task &&p) const {
  queue_->enqueue(std::forward<Task>(p));
}

}  // namespace symmetri
