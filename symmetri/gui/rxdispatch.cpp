#include "rxdispatch.h"

#include <iostream>

#include "blockingconcurrentqueue.h"
namespace rxdispatch {

static moodycamel::BlockingConcurrentQueue<model::Reducer> reducer_queue{10};

void push(model::Reducer&& r) {
  reducer_queue.enqueue(std::forward<model::Reducer>(r));
}

void dequeue(const rpp::dynamic_subscriber<model::Reducer>& sub) {
  model::Reducer f;
  sub.get_subscription().add([&]() {  // exit stuff
  });

  while (sub.is_subscribed()) {
    if (rxdispatch::reducer_queue.wait_dequeue_timed(
            f, std::chrono::milliseconds(16)) &&
        sub.is_subscribed()) {
      sub.on_next(f);
    }
  }
};
}  // namespace rxdispatch
