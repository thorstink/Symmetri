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
  Model m;  ///< The Petri net model
  std::atomic<std::optional<unsigned int>>
      thread_id_;     ///< The id of the thread from which run is called.
  const Marking m0_;  ///< The initial marking for this instance
  const Net net_;     ///< The net
  const PriorityTable priorities_;  ///< The priority table for this instance
  const std::vector<size_t>
      final_marking;  ///< The net will stop queueing reducers
                      ///< once the marking has been reached
  const std::shared_ptr<TaskSystem> stp;
  std::shared_ptr<BlockingConcurrentQueue<Reducer>> reducers;
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
        m0_(m0),
        net_(net),
        priorities_(priorities),
        final_marking(m.toTokens(final_marking)),
        stp(stp),
        reducers(std::make_shared<BlockingConcurrentQueue<Reducer>>(256)),
        case_id(case_id),
        early_exit(false) {}

  bool reset(const std::string &new_case_id) {
    if (thread_id_.load().has_value() || new_case_id == case_id) {
      return false;
    }

    case_id = new_case_id;
    reducers = std::make_shared<BlockingConcurrentQueue<Reducer>>(256);
    early_exit.store(false);
    thread_id_.store(std::nullopt);
    return true;
  }

  /**
   * @brief Get the Event Log object
   *
   * @return Eventlog
   */
  Eventlog getEventLog() const noexcept {
    const auto maybe_thread_id = thread_id_.load();
    if (maybe_thread_id && maybe_thread_id.value() != getThreadId()) {
      std::promise<Eventlog> el;
      std::future<Eventlog> el_getter = el.get_future();
      reducers->enqueue([&](Model &&model) {
        el.set_value(model.event_log);
        return std::ref(model);
      });
      return el_getter.get();
    } else {
      return m.event_log;
    }
  }

  std::pair<std::vector<Transition>, std::vector<Place>> getState()
      const noexcept {
    const auto maybe_thread_id = thread_id_.load();
    if (maybe_thread_id && maybe_thread_id.value() != getThreadId()) {
      std::promise<std::pair<std::vector<Transition>, std::vector<Place>>>
          state;
      auto getter = state.get_future();
      reducers->enqueue([&](Model &&model) {
        state.set_value(model.getState());
        return std::ref(model);
      });
      return getter.get();
    } else {
      return m.getState();
    }
  }

  std::vector<Transition> getFireableTransitions() const noexcept {
    const auto maybe_thread_id = thread_id_.load();
    if (maybe_thread_id && maybe_thread_id.value() != getThreadId()) {
      std::promise<std::vector<Transition>> transitions;
      std::future<std::vector<Transition>> transitions_getter =
          transitions.get_future();
      reducers->enqueue([&](Model &&model) {
        transitions.set_value(model.getFireableTransitions());
        return std::ref(model);
      });
      return transitions_getter.get();
    } else {
      return m.getFireableTransitions();
    }
  };

  void pause_() const noexcept {
    reducers->enqueue([&](Model &&model) {
      model.is_paused = true;
      return std::ref(model);
    });
  };

  void resume_() const noexcept {
    reducers->enqueue([&](Model &&model) {
      model.is_paused = false;
      return std::ref(model);
    });
  };

  /**
   * @brief run the petri net. This initializes the net with the initial
   * marking and blocks until it a) reached the final marking, b) deadlocked
   * or c) exited early.
   *
   * @return Result
   */
  Result fire() {
    // we are running!
    thread_id_.store(getThreadId());
    early_exit.store(false);
    m.event_log.clear();
    m.tokens_n = m.initial_tokens;

    bool waiting_for_resume = false;
    Reducer f;
    // start!
    m.fireTransitions(reducers, stp, true, case_id);
    // get a reducer. Immediately, or wait a bit
    while ((m.active_transitions_n.size() > 0 || m.is_paused) &&
           reducers->wait_dequeue_timed(f, -1)) {
      do {
        m = f(std::move(m));
      } while (!early_exit.load() && reducers->try_dequeue(f));

      if (MarkingReached(m.tokens_n, final_marking) || early_exit.load()) {
        // we're done
        break;
      } else if (!m.is_paused && !waiting_for_resume) {
        // we're firing
        m.fireTransitions(reducers, stp, true, case_id);
      } else if (m.is_paused && !waiting_for_resume) {
        // we've been asked to pause
        waiting_for_resume = true;
        for (const auto transition_index : m.active_transitions_n) {
          pause(m.net.store.at(transition_index));
        }
      } else if (!m.is_paused && waiting_for_resume) {
        // we've been asked to resume
        waiting_for_resume = false;
        for (const auto transition_index : m.active_transitions_n) {
          resume(m.net.store.at(transition_index));
        }
        m.fireTransitions(reducers, stp, true, case_id);
      }
    }

    // determine what was the reason we terminated.
    State result;
    if (early_exit.load()) {
      {
        // populate that eventlog with child eventlog and possible cancelations.
        for (const auto transition_index : m.active_transitions_n) {
          auto [el, state] = cancel(m.net.store.at(transition_index));
          if (!el.empty()) {
            m.event_log.insert(m.event_log.end(), el.begin(), el.end());
          }
          m.event_log.push_back({case_id, m.net.transition[transition_index],
                                 state, Clock::now()});
        }
        result = State::UserExit;
      }
    } else if (MarkingReached(m.tokens_n, final_marking)) {
      result = State::Completed;
    } else if (m.active_transitions_n.empty()) {
      result = State::Deadlock;
    } else {
      result = State::Error;
    }

    Result res = {m.event_log, result};
    while (reducers->try_dequeue(f)) {
      m = f(std::move(m));
    }
    thread_id_.store(std::nullopt);
    return res;
  }
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
            if (impl->thread_id_.load()) {
              impl->reducers->enqueue([=](Model &&m) {
                const auto t_index = toIndex(m.net.transition, t);
                m.active_transitions_n.push_back(t_index);
                impl->reducers->enqueue(createReducerForTransition(
                    t_index, m.net.transition[t_index], m.net.store[t_index],
                    impl->case_id, impl->reducers));
                return std::ref(m);
              });
            }
          }};
}

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

bool PetriNet::tryFireTransition(const Transition &t) const noexcept {
  if (impl == nullptr) {
    return false;
  }
  Reducer f;
  while (impl->reducers->try_dequeue(f)) {
    impl->m = f(std::move(impl->m));
  }
  return impl->m.fire(t, impl->reducers, impl->stp, "manual");
};

Result PetriNet::fire() const noexcept {
  if (impl == nullptr) {
    spdlog::error("Something went seriously wrong. Please send a bug report.");
    return {{}, State::Error};
  } else if (impl->thread_id_.load()) {
    spdlog::warn(
        "[{0}] is already active, it can not run it again before it is "
        "finished.",
        impl->case_id);
    return {{}, State::Error};
  } else {
    return impl->fire();
  }
}

Eventlog PetriNet::getEventLog() const noexcept { return impl->getEventLog(); };

std::pair<std::vector<Transition>, std::vector<Place>> PetriNet::getState()
    const noexcept {
  return impl->getState();
}

std::vector<Transition> PetriNet::getFireableTransitions() const noexcept {
  return impl->getFireableTransitions();
};

std::function<void()> PetriNet::registerTransitionCallback(
    const Transition &transition) const noexcept {
  return [transition, this] { register_functor(transition); };
}

Result PetriNet::cancel() const noexcept {
  const auto maybe_thread_id = impl->thread_id_.load();
  if (maybe_thread_id && maybe_thread_id.value() != getThreadId() &&
      !impl->early_exit.load()) {
    std::mutex m;
    std::condition_variable cv;
    bool ready = false;
    impl->reducers->enqueue([=, &m, &cv, &ready](Model &&model) {
      impl->early_exit.store(true);
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
    impl->reducers->enqueue([=](Model &&model) {
      impl->early_exit.store(true);
      return std::ref(model);
    });
  }

  return {getEventLog(), State::UserExit};
}

void PetriNet::pause() const noexcept { impl->pause_(); }

void PetriNet::resume() const noexcept { impl->resume_(); }

bool PetriNet::reuseApplication(const std::string &new_case_id) {
  return impl->reset(new_case_id);
}

}  // namespace symmetri
