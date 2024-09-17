#include "rxdispatch.h"

#include "blockingconcurrentqueue.h"
namespace rxdispatch {

static moodycamel::BlockingConcurrentQueue<model::Reducer> reducer_queue{10};

void unsubscribe() {
  push(model::Reducer([](auto&& m) { return m; }));
}

void push(model::Reducer&& r) {
  reducer_queue.enqueue(std::forward<model::Reducer>(r));
}

void dequeue(rpp::dynamic_observer<model::Reducer>&& observer) {
  model::Reducer f;
  while (not observer.is_disposed()) {
    rxdispatch::reducer_queue.wait_dequeue(f);
    observer.on_next(std::move(f));
  }
};
}  // namespace rxdispatch
