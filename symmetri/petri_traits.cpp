#include <future>

#include "petri.h"
#include "symmetri/symmetri.h"
#include "symmetri/utilities.hpp"

namespace symmetri {

/**
 * @brief Get the Thread Id object. We go to "great lengths" to get a 32 bit
 * representation of the thread id, because in that case the atomic is lock-free
 * on (most) 64 bit systems.
 *
 * @return unsigned int
 */
unsigned int getThreadId() {
  return static_cast<unsigned int>(
      std::hash<std::thread::id>()(std::this_thread::get_id()));
}

Token fire(const PetriNet &app) {
  if (app.impl->thread_id_.load().has_value()) {
    return Color::Failed;
  }
  auto &m = *app.impl;
  m.thread_id_.store(getThreadId());
  m.scheduled_callbacks.clear();
  m.tokens = m.initial_tokens;
  m.state = Color::Started;
  Reducer f;
  while (m.reducer_queue->try_dequeue(f)) { /* get rid of old reducers  */
  }

  // start!
  m.reducer_queue->enqueue([=](Petri &) {});
  while ((m.state == Color::Started || m.state == Color::Paused) &&
         m.reducer_queue->wait_dequeue_timed(f, -1)) {
    do {
      f(m);
    } while (m.reducer_queue->try_dequeue(f));

    if (MarkingReached(m.tokens, m.final_marking)) {
      m.state = Color::Success;
    }

    if (m.state == Color::Started) {
      // we're firing
      m.fireTransitions();
      // if there's nothing to fire; we deadlocked
      if (m.scheduled_callbacks.size() == 0) {
        m.state = Color::Deadlocked;
      } else if (MarkingReached(m.tokens, m.final_marking)) {
        m.state = Color::Success;
      }
    }
  }

  if (MarkingReached(m.tokens, m.final_marking)) {
    m.state = Color::Success;
  }

  while (!m.scheduled_callbacks.empty()) {
    if (m.reducer_queue->wait_dequeue_timed(f, std::chrono::milliseconds(10))) {
      f(m);
    }
  }

  m.thread_id_.store(std::nullopt);

  return m.state;
}

void cancel(const PetriNet &app) {
  app.impl->reducer_queue->enqueue([=](Petri &model) {
    model.state = Color::Canceled;
    for (const auto transition_index : model.scheduled_callbacks) {
      cancel(model.net.store.at(transition_index));
      model.log.push_back({transition_index, Color::Canceled, Clock::now()});
    }
  });
}

void pause(const PetriNet &app) {
  app.impl->reducer_queue->enqueue([](Petri &model) {
    model.state = Color::Paused;
    for (const auto transition_index : model.scheduled_callbacks) {
      pause(model.net.store.at(transition_index));
    }
  });
}

void resume(const PetriNet &app) {
  app.impl->reducer_queue->enqueue([](Petri &model) {
    model.state = Color::Started;
    for (const auto transition_index : model.scheduled_callbacks) {
      resume(model.net.store.at(transition_index));
    }
  });
}

Eventlog getLog(const PetriNet &app) {
  if (app.impl->thread_id_.load()) {
    std::promise<Eventlog> el;
    std::future<Eventlog> el_getter = el.get_future();
    app.impl->reducer_queue->enqueue(
        [&](Petri &model) { el.set_value(model.getLogInternal()); });
    return el_getter.get();
  } else {
    return app.impl->getLogInternal();
  }
}

}  // namespace symmetri
