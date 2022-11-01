#include "Symmetri/actions.h"

#include <blockingconcurrentqueue.h>
#include <spdlog/spdlog.h>

namespace symmetri {
static moodycamel::BlockingConcurrentQueue<PolyAction> actions(256);
const PolyAction noop([] {});

StoppablePool::StoppablePool(unsigned int thread_count)
    : pool(thread_count), stop_flag(false) {
  spdlog::info("Create pool");

  std::generate(std::begin(pool), std::end(pool),
                [this] { return std::thread(&StoppablePool::loop, this); });
}

void StoppablePool::join() {
  for (auto &&t : pool) {
    spdlog::info("join pool");

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
      if (stop_flag.load() == true) {
        break;
      }
      runTransition(transition);
      transition = noop;
    }
  } while (stop_flag.load() == false);
}

void StoppablePool::enqueue(const PolyAction &p) { actions.enqueue(p); }

void StoppablePool::stop() {
  stop_flag.store(true);
  for (size_t i = 0; i < pool.size(); ++i) {
    actions.enqueue([] {});
  }
  join();
}

}  // namespace symmetri
