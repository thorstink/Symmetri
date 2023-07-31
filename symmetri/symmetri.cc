#include "symmetri/symmetri.h"

#include <blockingconcurrentqueue.h>
#include <spdlog/spdlog.h>

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>
#include <tuple>

#include "model.h"
#include "symmetri/parsers.h"

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

using namespace moodycamel;

bool areAllTransitionsInStore(const Store &store, const Net &net) noexcept {
  return std::all_of(net.cbegin(), net.cend(), [&store](const auto &p) {
    const auto &t = std::get<0>(p);
    bool store_has_transition =
        std::find_if(store.begin(), store.end(), [&](const auto &e) {
          return e.first == t;
        }) != store.end();
    if (!store_has_transition) {
      spdlog::error("Transition {0} is not in store", t);
    }
    return store_has_transition;
  });
}

/**
 * @brief Petri is the class that holds the implementation of the Petri net. It
 * holds pointers to the reducer queue and that thread pool. Calling `fire()` on
 * this class will do the *actual* execution of the Petri net.
 *
 */
struct Petri {
  Petri(Petri const &) = delete;
  Petri(Petri &&) noexcept = delete;
  Petri &operator=(Petri const &) = delete;
  Petri &operator=(Petri &&) noexcept = delete;

  Model m;  ///< The Petri net model
  std::atomic<std::optional<unsigned int>>
      thread_id_;  ///< The id of the thread from which run is called.
  const std::vector<size_t>
      final_marking;  ///< The net will stop queueing reducers
                      ///< once the marking has been reached
  const std::shared_ptr<TaskSystem> stp;
  std::array<std::shared_ptr<BlockingConcurrentQueue<Reducer>>, 2> reducers;
  unsigned int reducer_selector;
  std::string case_id;  ///< The case id of this particular Petri instance
  std::atomic<bool> early_exit;  ///< once it is true, no more new transitions
                                 ///< will be queued and the run will exit.

  /**
   * @brief Construct a new Petri object. Most importantly, it also creates the
   * reducer queue and exposes the `run` function to actually run the petri
   * net.
   *
   * @param net
   * @param m0
   * @param stp
   * @param final_marking
   * @param store
   * @param priority
   * @param case_id
   */
  Petri(const Net &net, const Marking &m0, std::shared_ptr<TaskSystem> stp,
        const Marking &final_marking, const Store &store,
        const PriorityTable &priorities, const std::string &case_id)
      : m(net, store, priorities, m0),
        thread_id_(std::nullopt),
        final_marking(m.toTokens(final_marking)),
        stp(stp),
        reducers({std::make_shared<BlockingConcurrentQueue<Reducer>>(32),
                  std::make_shared<BlockingConcurrentQueue<Reducer>>(32)}),
        reducer_selector(0),
        case_id(case_id),
        early_exit(false) {}

  const std::shared_ptr<BlockingConcurrentQueue<Reducer>> &setFreshQueue() {
    // increment index to get the already prepared queue.
    reducer_selector = reducer_selector > 0 ? 0 : 1;
    // create a new queue for later use at the next index.
    stp->push([reducer = reducers[reducer_selector > 0 ? 0 : 1]]() mutable {
      reducer.reset(new BlockingConcurrentQueue<Reducer>{32});
    });
    return reducers[reducer_selector];
  }

  std::vector<Transition> getFireableTransitions() const noexcept {
    const auto maybe_thread_id = thread_id_.load();
    if (maybe_thread_id && maybe_thread_id.value() != getThreadId()) {
      std::promise<std::vector<Transition>> transitions;
      std::future<std::vector<Transition>> transitions_getter =
          transitions.get_future();
      reducers[reducer_selector]->enqueue([&](Model &&model) {
        transitions.set_value(model.getFireableTransitions());
        return std::ref(model);
      });
      return transitions_getter.get();
    } else {
      return m.getFireableTransitions();
    }
  };
};

/**
 * @brief A factory function that creates a Petri and a handler that allows to
 * register triggers to functions.
 *
 * @param net
 * @param m0
 * @param final_marking
 * @param store
 * @param priority
 * @param case_id
 * @param stp
 * @return std::tuple<std::shared_ptr<Petri>, std::function<void(const
 * Transition &)>>
 */
std::tuple<std::shared_ptr<Petri>, std::function<void(const Transition &)>>
create(const Net &net, const Marking &m0, const Marking &final_marking,
       const Store &store, const PriorityTable &priority,
       const std::string &case_id, std::shared_ptr<TaskSystem> stp) {
  auto impl = std::make_shared<Petri>(net, m0, stp, final_marking, store,
                                      priority, case_id);
  return {impl, [=](const Transition &t) {
            const auto &reducer = impl->reducers[impl->reducer_selector];
            reducer->enqueue([=](Model &&m) {
              const auto t_index = toIndex(m.net.transition, t);
              m.active_transitions_n.push_back(t_index);
              reducer->enqueue(
                  fireTransition(t_index, m.net.transition[t_index],
                                 m.net.store[t_index], impl->case_id, reducer));
              return std::ref(m);
            });
          }};
}

}  // namespace symmetri
using namespace symmetri;

PetriNet::PetriNet(const std::set<std::string> &files,
                   const Marking &final_marking, const Store &store,
                   const PriorityTable &priorities, const std::string &case_id,
                   std::shared_ptr<TaskSystem> stp) {
  const auto [net, m0] = readPnml(files);
  if (areAllTransitionsInStore(store, net)) {
    std::tie(impl, register_functor) =
        create(net, m0, final_marking, store, priorities, case_id, stp);
  }
}

PetriNet::PetriNet(const std::set<std::string> &files,
                   const Marking &final_marking, const Store &store,
                   const std::string &case_id,
                   std::shared_ptr<TaskSystem> stp) {
  const auto [net, m0, priorities] = readGrml(files);
  if (areAllTransitionsInStore(store, net)) {
    std::tie(impl, register_functor) =
        create(net, m0, final_marking, store, priorities, case_id, stp);
  }
}

PetriNet::PetriNet(const Net &net, const Marking &m0,
                   const Marking &final_marking, const Store &store,
                   const PriorityTable &priorities, const std::string &case_id,
                   std::shared_ptr<TaskSystem> stp) {
  if (areAllTransitionsInStore(store, net)) {
    std::tie(impl, register_functor) =
        create(net, m0, final_marking, store, priorities, case_id, stp);
  }
}

std::function<void()> PetriNet::registerTransitionCallback(
    const Transition &transition) const noexcept {
  return [transition, this] { register_functor(transition); };
}

bool PetriNet::reuseApplication(const std::string &new_case_id) {
  if (impl) {
    auto &petri = *impl;
    if (petri.thread_id_.load().has_value() || new_case_id == petri.case_id) {
      return false;
    }
    petri.case_id = new_case_id;
    petri.thread_id_.store(std::nullopt);
    return true;
  } else {
    return false;
  }
}

symmetri::Result fire(const PetriNet &app) {
  if (app.impl == nullptr) {
    spdlog::error("Something went seriously wrong. Please send a bug report.");
    return {{}, symmetri::State::Error};
  }

  auto &petri = *app.impl;
  auto &m = petri.m;
  if (app.impl->thread_id_.load()) {
    spdlog::warn(
        "[{0}] is already active, it can not run it again before it is "
        "finished.",
        app.impl->case_id);
    return {{}, symmetri::State::Error};
  }

  // we are running!
  auto &active_reducers = petri.reducers[petri.reducer_selector];
  petri.thread_id_.store(symmetri::getThreadId());
  petri.early_exit.store(false);
  m.event_log.clear();
  m.tokens_n = m.initial_tokens;

  bool waiting_for_resume = false;
  symmetri::Reducer f;
  // start!
  m.fireTransitions(active_reducers, petri.stp, true, petri.case_id);
  // get a reducer. Immediately, or wait a bit
  while ((m.active_transitions_n.size() > 0 || m.is_paused) &&
         active_reducers->wait_dequeue_timed(f, -1)) {
    do {
      m = f(std::move(m));
    } while (!petri.early_exit.load() && active_reducers->try_dequeue(f));

    if (symmetri::MarkingReached(m.tokens_n, petri.final_marking) ||
        petri.early_exit.load()) {
      // we're done
      break;
    } else if (!m.is_paused && !waiting_for_resume) {
      // we're firing
      m.fireTransitions(active_reducers, petri.stp, true, petri.case_id);
    } else if (m.is_paused && !waiting_for_resume) {
      // we've been asked to pause
      waiting_for_resume = true;
      for (const auto transition_index : m.active_transitions_n) {
        pause(m.net.store.at(transition_index));
      }
    } else if (!m.is_paused && waiting_for_resume) {
      // we've been asked to resume
      waiting_for_resume = false;
      m.fireTransitions(active_reducers, petri.stp, true, petri.case_id);
      for (const auto transition_index : m.active_transitions_n) {
        resume(m.net.store.at(transition_index));
      }
    }
  }

  // determine what was the reason we terminated.
  symmetri::State result;
  if (petri.early_exit.load()) {
    {
      // populate that eventlog with child eventlog and possible cancelations.
      for (const auto transition_index : m.active_transitions_n) {
        auto [el, state] = cancel(m.net.store.at(transition_index));
        if (!el.empty()) {
          m.event_log.insert(m.event_log.end(), el.begin(), el.end());
        }
        m.event_log.push_back({petri.case_id,
                               m.net.transition[transition_index], state,
                               symmetri::Clock::now()});
      }
      result = symmetri::State::UserExit;
    }
  } else if (symmetri::MarkingReached(m.tokens_n, petri.final_marking)) {
    result = symmetri::State::Completed;
  } else if (m.active_transitions_n.empty()) {
    result = symmetri::State::Deadlock;
  } else {
    result = symmetri::State::Error;
  }

  // empty reducers
  m.active_transitions_n.clear();
  while (active_reducers->try_dequeue(f)) {
    m = f(std::move(m));
  }

  petri.thread_id_.store(std::nullopt);
  petri.setFreshQueue();

  return {m.event_log, result};
};

symmetri::Result cancel(const PetriNet &app) {
  const auto &impl = *app.impl;
  const auto maybe_thread_id = impl.thread_id_.load();
  if (maybe_thread_id && maybe_thread_id.value() != symmetri::getThreadId() &&
      !impl.early_exit.load()) {
    std::mutex m;
    std::condition_variable cv;
    bool ready = false;
    impl.reducers[impl.reducer_selector]->enqueue(
        [=, &m, &cv, &ready](symmetri::Model &&model) {
          app.impl->early_exit.store(true);
          {
            std::lock_guard lk(m);
            ready = true;
          }
          cv.notify_one();
          return std::ref(model);
        });
    std::unique_lock lk(m);
    cv.wait(lk, [&] { return ready; });
  } else {
    impl.reducers[impl.reducer_selector]->enqueue([=](symmetri::Model &&model) {
      app.impl->early_exit.store(true);
      return std::ref(model);
    });
  }

  return {getLog(app), symmetri::State::UserExit};
}

void pause(const PetriNet &app) {
  app.impl->reducers[app.impl->reducer_selector]->enqueue(
      [&](symmetri::Model &&model) {
        model.is_paused = true;
        return std::ref(model);
      });
};

void resume(const PetriNet &app) {
  app.impl->reducers[app.impl->reducer_selector]->enqueue(
      [&](symmetri::Model &&model) {
        model.is_paused = false;
        return std::ref(model);
      });
};

symmetri::Eventlog getLog(const PetriNet &app) {
  const auto &impl = *app.impl;
  const auto maybe_thread_id = impl.thread_id_.load();
  if (maybe_thread_id && maybe_thread_id.value() != getThreadId()) {
    std::promise<Eventlog> el;
    std::future<Eventlog> el_getter = el.get_future();
    impl.reducers[impl.reducer_selector]->enqueue([&](Model &&model) {
      auto eventlog = model.event_log;
      // get event log from parent nets:
      for (size_t pt_idx : model.active_transitions_n) {
        auto sub_el = getLog(model.net.store[pt_idx]);
        if (!sub_el.empty()) {
          eventlog.insert(eventlog.end(), sub_el.begin(), sub_el.end());
        }
      }
      el.set_value(std::move(eventlog));
      return std::ref(model);
    });
    return el_getter.get();
  } else {
    return impl.m.event_log;
  }
  getLog(app);
}
