#include "rxdispatch.h"

#include "blockingconcurrentqueue.h"
namespace rxdispatch {

moodycamel::BlockingConcurrentQueue<model::Reducer> reducer_queue{10};

void push(model::Reducer&& r) {
  reducer_queue.enqueue(std::forward<model::Reducer>(r));
}

void dequeue(const rpp::dynamic_subscriber<model::Reducer>& sub) {
  model::Reducer f;
  sub.get_subscription().add([&]() {
    // cleanup?
  });

  while (sub.is_subscribed() &&
         rxdispatch::reducer_queue.wait_dequeue_timed(f, -1)) {
    sub.on_next(f);
  }
};
}  // namespace rxdispatch
