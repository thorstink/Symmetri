#include "rxdispatch.h"

#include "blockingconcurrentqueue.h"
namespace rxdispatch {

static moodycamel::BlockingConcurrentQueue<model::Reducer> reducer_queue{10};

moodycamel::BlockingConcurrentQueue<model::Reducer>& getQueue() {
  return reducer_queue;
}

void unsubscribe() {
  push(model::Reducer([](auto&& m) { return m; }));
}

void push(model::Reducer&& r) {
  reducer_queue.enqueue(std::forward<model::Reducer>(r));
}

}  // namespace rxdispatch
