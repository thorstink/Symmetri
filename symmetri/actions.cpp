#include "symmetri/actions.h"

#include <blockingconcurrentqueue.h>
#include <spdlog/spdlog.h>

namespace symmetri {
const PolyAction noop([] {});

static moodycamel::BlockingConcurrentQueue<PolyAction> actions(256);

StoppablePool::StoppablePool(unsigned int thread_count)
    : pool(thread_count), stop_flag(false) {
  std::generate(std::begin(pool), std::end(pool),
                [this] { return std::thread(&StoppablePool::loop, this); });
  spdlog::info("Created pool with {0} threads.", thread_count);
}

StoppablePool::~StoppablePool() {
  stop_flag.store(true, std::memory_order_seq_cst);
  for (size_t i = 0; i < pool.size(); ++i) {
    actions.enqueue([] {});
  }
  for (auto &&t : pool) {
    if (t.joinable()) {
      t.join();
    }
  }
}

void StoppablePool::loop() {
  PolyAction transition(noop);
  do {
    if (actions.wait_dequeue_timed(transition,
                                   std::chrono::milliseconds(250))) {
      if (stop_flag.load(std::memory_order_relaxed) == true) {
        break;
      }
      runTransition(transition);
      transition = noop;
    }
  } while (stop_flag.load(std::memory_order_relaxed) == false);
}

void StoppablePool::enqueue(PolyAction &&p) const {
  actions.enqueue(std::forward<PolyAction>(p));
}

}  // namespace symmetri
