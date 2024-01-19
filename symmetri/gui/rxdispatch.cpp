#include "rxdispatch.h"

#include "blockingconcurrentqueue.h"
namespace rxdispatch {

static moodycamel::BlockingConcurrentQueue<model::Reducer> reducer_queue{10};

void push(model::Reducer&& r) {
  reducer_queue.enqueue(std::forward<model::Reducer>(r));
}

void dequeue(const rpp::dynamic_subscriber<model::Reducer>& sub) {
  model::Reducer f;
  sub.get_subscription().add([&]() {
    printf("%s", "x-");  // x is notation for unsubscribed
    rxdispatch::reducer_queue.enqueue(model::noop);
  });

  while (sub.is_subscribed()) {
    while (sub.is_subscribed() && rxdispatch::reducer_queue.wait_dequeue_timed(
                                      f, std::chrono::milliseconds(100))) {
      sub.on_next(f);
    }
  }
};
}  // namespace rxdispatch
