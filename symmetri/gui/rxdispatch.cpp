#include "rxdispatch.h"

#include <iostream>

#include "blockingconcurrentqueue.h"
namespace rxdispatch {

static moodycamel::BlockingConcurrentQueue<model::Reducer> reducer_queue{10};
static auto subscription = rpp::composite_disposable_wrapper::make();

void unsubscribe() {
  push(model::Reducer([](auto& m) {
    subscription.dispose();
    return m;
  }));
}

void push(model::Reducer&& r) {
  reducer_queue.enqueue(std::forward<model::Reducer>(r));
}

void dequeue(rpp::dynamic_observer<model::Reducer>&& observer) {
  model::Reducer f;
  observer.set_upstream(rpp::disposable_wrapper{subscription});
  while (not observer.is_disposed()) {
    rxdispatch::reducer_queue.wait_dequeue(f);
    observer.on_next(std::move(f));
  }
};
}  // namespace rxdispatch
