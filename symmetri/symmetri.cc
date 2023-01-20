#include "symmetri/symmetri.h"

#include <blockingconcurrentqueue.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <condition_variable>
#include <functional>
#include <memory>
#include <sstream>

#include "model.h"
#include "symmetri/pnml_parser.h"
namespace symmetri {

std::condition_variable cv;
std::mutex cv_m;  // This mutex is used for two purposes:
                  // 1) to synchronize accesses to PAUSE & EARLY_EXIT
                  // 2) for the condition variable cv

std::atomic<bool> PAUSE(false);
std::atomic<bool> EARLY_EXIT(false);

// The default exit handler just sets early exit to true.
std::function<void()> EARLY_EXIT_HANDLER = []() {
  spdlog::info("User requests exit");
  std::lock_guard<std::mutex> lk(cv_m);
  EARLY_EXIT.store(true);
  cv.notify_all();
};

void blockIfPaused(const std::string &case_id) {
  std::unique_lock<std::mutex> lk(cv_m);
  if (PAUSE.load(std::memory_order_relaxed)) {
    spdlog::get(case_id)->info("Execution is paused");
    cv.wait(lk, [] { return !PAUSE.load() || EARLY_EXIT.load(); });
    spdlog::get(case_id)->info("Execution is resumed");
  }
}

// Define the function to be called when ctrl-c (SIGINT) is sent to process
inline void exit_handler(int) noexcept { EARLY_EXIT_HANDLER(); }

using namespace moodycamel;

bool check(const Store &store, const symmetri::StateNet &net) noexcept {
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

struct Impl {
  Model m;
  const symmetri::NetMarking m0_;
  BlockingConcurrentQueue<Reducer> reducers;
  const symmetri::StoppablePool &stp;
  const std::string case_id;
  std::atomic<bool> active;
  std::optional<symmetri::NetMarking> final_marking;
  Impl(const symmetri::StateNet &net, const symmetri::NetMarking &m0,
       const symmetri::StoppablePool &stp,
       const std::optional<symmetri::NetMarking> &final_marking,
       const Store &store,
       const std::vector<std::pair<symmetri::Transition, int8_t>> &priority,
       const std::string &case_id)
      : m(net, store, priority, m0),
        m0_(m0),
        reducers(256),
        stp(stp),
        case_id(case_id),
        active(false),
        final_marking(final_marking) {
    EARLY_EXIT_HANDLER = [this]() {
      spdlog::info("User requests exit");
      std::lock_guard<std::mutex> lk(cv_m);
      if (!EARLY_EXIT.load()) {
        EARLY_EXIT.store(true);
      }
      reducers.enqueue([](Model &&model) -> Model & { return model; });
      cv.notify_all();
    };
  }
  const Model &getModel() const { return m; }
  symmetri::Eventlog getEventLog() const { return m.event_log; }
  TransitionResult run() {
    // todo.. not have to assign it manually to reset.
    m.event_log = {};
    m.tokens_n = m.initial_tokens;
    std::vector<size_t> final_tokens;
    if (final_marking.has_value()) {
      for (const auto &[p, c] : final_marking.value()) {
        for (int i = 0; i < c; i++) {
          final_tokens.push_back(toIndex(m.net.place, p));
        }
      }
    }

    // this is the check whether we should break the loop. It's either early
    // exit, otherwise there must be event_logs.
    auto stop_condition = [&] {
      return EARLY_EXIT.load(std::memory_order_relaxed) ||
             (m.active_transitions_n.size() == 0 && !m.event_log.empty());
    };
    active.store(true);
    Reducer f;
    // start!
    m = runTransitions(m, reducers, stp, true, case_id);
    // get a reducer. Immediately, or wait a bit
    while (!stop_condition() && reducers.wait_dequeue_timed(f, -1) &&
           !stop_condition()) {
      do {
        m = f(std::move(m));
      } while (reducers.try_dequeue(f));
      blockIfPaused(case_id);
      if (MarkingReached(m.tokens_n, final_tokens)) {
        break;
      }
      m = runTransitions(m, reducers, stp, true, case_id);
    }

    active.store(false);

    // determine what was the reason we terminated.
    TransitionState result;
    if (EARLY_EXIT.load()) {
      result = TransitionState::UserExit;
    } else if (MarkingReached(m.tokens_n, final_tokens)) {
      result = TransitionState::Completed;
    } else if (m.active_transitions_n.empty()) {
      result = TransitionState::Deadlock;
    } else {
      result = TransitionState::Error;
    }
    return {m.event_log, result};
  }
};

Application::Application(
    const std::set<std::string> &files,
    const std::optional<symmetri::NetMarking> &final_marking,
    const Store &store,
    const std::vector<std::pair<symmetri::Transition, int8_t>> &priority,
    const std::string &case_id, const symmetri::StoppablePool &stp) {
  const auto &[net, m0] = readPetriNets(files);
  createApplication(net, m0, final_marking, store, priority, case_id, stp);
}

Application::Application(
    const symmetri::StateNet &net, const symmetri::NetMarking &m0,
    const std::optional<symmetri::NetMarking> &final_marking,
    const Store &store,
    const std::vector<std::pair<symmetri::Transition, int8_t>> &priority,
    const std::string &case_id, const symmetri::StoppablePool &stp) {
  createApplication(net, m0, final_marking, store, priority, case_id, stp);
}

void Application::createApplication(
    const symmetri::StateNet &net, const symmetri::NetMarking &m0,
    const std::optional<symmetri::NetMarking> &final_marking,
    const Store &store,
    const std::vector<std::pair<symmetri::Transition, int8_t>> &priority,
    const std::string &case_id, const symmetri::StoppablePool &stp) {
  signal(SIGINT, exit_handler);
  std::stringstream s;
  s << "[%Y-%m-%d %H:%M:%S.%f] [%^%l%$] [thread %t] [" << case_id << "] %v";
  auto console = spdlog::stdout_color_mt(case_id);

  console->set_pattern(s.str());
  if (check(store, net)) {
    // init
    impl = std::make_shared<Impl>(net, m0, stp, final_marking, store, priority,
                                  case_id);
    // register a function that "forces" transitions into the queue.
    p = [this](const std::string &t) {
      if (impl->active.load()) {
        impl->stp.enqueue([this, t_index = toIndex(impl->m.net.transition, t)] {
          // this should be cleaned the next iteration
          impl->m.active_transitions_n.push_back(t_index);
          impl->reducers.enqueue(createReducerForTransition(
              t_index, impl->m.net.store[t_index], impl->case_id));
        });
      }
    };
  }
}

TransitionResult Application::operator()() const noexcept {
  if (impl == nullptr) {
    spdlog::error("Error not all transitions are in the store");
    return {{}, TransitionState::Error};
  } else {
    auto res = impl->run();
    return res;
  }
}

symmetri::Eventlog Application::getEvenLog() const noexcept {
  return impl->getEventLog();
};
std::vector<symmetri::Place> Application::getMarking() const noexcept {
  return impl->getModel().getMarking();
};
std::vector<std::string> Application::getActiveTransitions() const noexcept {
  return impl->getModel().getActiveTransitions();
};

std::function<void()> Application::registerTransitionCallback(
    const std::string &transition) const noexcept {
  return [transition, this]() { p(transition); };
}

void Application::exitEarly() const noexcept { EARLY_EXIT_HANDLER(); }

void Application::togglePause() const noexcept {
  std::lock_guard<std::mutex> lk(cv_m);
  PAUSE.store(!PAUSE.load());
  cv.notify_all();
}

}  // namespace symmetri
