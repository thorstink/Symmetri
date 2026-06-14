#include "rxdispatch.h"

#include "blockingconcurrentqueue.h"
#include "gui/model.h"

namespace rxdispatch {

static moodycamel::BlockingConcurrentQueue<Update> reducer_queue{10};

moodycamel::BlockingConcurrentQueue<Update>& getQueue() {
  return reducer_queue;
}

void unsubscribe() {
  // Push a no-op so the worker thread blocked on wait_dequeue wakes and can
  // observe disposal.
  pushView([](model::ViewState&, const model::EditState&) {});
}

void push(Update&& r) {
  reducer_queue.enqueue(std::move(r));
  if (on_push) on_push();
}

void pushEdit(model::EditReducer r) { push(Update{std::move(r)}); }
void pushView(model::ViewReducer r) { push(Update{std::move(r)}); }

}  // namespace rxdispatch
