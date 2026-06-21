#include "rxdispatch.h"

#include <vector>

#include "blockingconcurrentqueue.h"
#include "gui/model.h"

namespace rxdispatch {

static moodycamel::BlockingConcurrentQueue<Update> reducer_queue{10};

// ---------------------------------------------------------------------------
// Batch implementation
// ---------------------------------------------------------------------------

namespace {

struct BatchStorage {
  model::EditReducer edit;
  std::vector<model::ViewReducer> views;
};

thread_local int batch_depth = 0;
thread_local BatchStorage batch_store;

}  // namespace

Batch::Batch() { ++batch_depth; }

Batch::~Batch() {
  if (--batch_depth > 0) return;  // inner scope; let the outermost flush
  if (batch_store.edit) push(Update{std::move(batch_store.edit)});
  if (!batch_store.views.empty()) {
    auto vs = std::move(batch_store.views);
    push(Update{model::ViewReducer{
        [vs = std::move(vs)](model::ViewState& v, const model::EditState& e) {
          for (const auto& r : vs) r(v, e);
        }}});
  }
  batch_store.edit = nullptr;
  batch_store.views.clear();
}

// ---------------------------------------------------------------------------

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

void pushEdit(model::EditReducer r) {
  if (batch_depth > 0) {
    if (!batch_store.edit) {
      batch_store.edit = std::move(r);
    } else {
      auto prev = std::move(batch_store.edit);
      batch_store.edit = [p = std::move(prev),
                          n = std::move(r)](model::EditState&& e) {
        return n(p(std::move(e)));
      };
    }
  } else {
    push(Update{std::move(r)});
  }
}

void pushView(model::ViewReducer r) {
  if (batch_depth > 0) {
    batch_store.views.push_back(std::move(r));
  } else {
    push(Update{std::move(r)});
  }
}

}  // namespace rxdispatch
