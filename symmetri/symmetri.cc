#include "symmetri/symmetri.h"

#include <blockingconcurrentqueue.h>
#include <signal.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include <condition_variable>
#include <fstream>
#include <functional>
#include <memory>
#include <numeric>
#include <optional>
#include <sstream>
#include <tuple>

#include "model.h"
#include "pnml_parser.h"
namespace symmetri {

std::condition_variable cv;
std::mutex cv_m;  // This mutex is used for two purposes:
                  // 1) to synchronize accesses to PAUSE
                  // 2) for the condition variable cv
std::atomic<bool> PAUSE(false);

void blockIfPaused(const std::string &case_id) {
  std::unique_lock<std::mutex> lk(cv_m);
  if (PAUSE.load(std::memory_order_relaxed)) {
    spdlog::get(case_id)->info("Execution is paused");
    cv.wait(lk, [] { return !PAUSE.load(); });
    spdlog::get(case_id)->info("Execution is resumed");
  }
}

// Define the function to be called when ctrl-c (SIGINT) is sent to process
std::atomic<bool> EARLY_EXIT(false);
inline void signal_handler(int signal) noexcept {
  spdlog::info("User requests exit");
  EARLY_EXIT.store(true);
}

using namespace moodycamel;

size_t calculateTrace(Eventlog event_log) noexcept {
  // calculate a trace-id, in a simple way.
  return std::hash<std::string>{}(std::accumulate(
      event_log.begin(), event_log.end(), std::string(""),
      [](const auto &acc, const Event &n) {
        return n.state == TransitionState::Completed ? acc + n.transition : acc;
      }));
}

std::string printState(symmetri::TransitionState s) noexcept {
  std::string ret;
  switch (s) {
    case TransitionState::Started:
      ret = "Started";
      break;
    case TransitionState::Completed:
      ret = "Completed";
      break;
    case TransitionState::Deadlock:
      ret = "Deadlock";
      break;
    case TransitionState::UserExit:
      ret = "UserExit";
      break;
    case TransitionState::Error:
      ret = "Error";
      break;
  }

  return ret;
}

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
       const Store &store, const std::string &case_id)
      : m(net, store, m0),
        m0_(m0),
        reducers(256),
        stp(stp),
        case_id(case_id),
        active(false),
        final_marking(final_marking) {}
  const Model &getModel() const { return m; }
  symmetri::Eventlog getEventLog() const { return m.event_log; }
  TransitionResult run() {
    // todo.. not have to assign it manually to reset.
    m.M = m0_;
    m.event_log = {};
    m.tokens = {};
    for (auto [p, c] : m0_) {
      for (int i = 0; i < c; i++) {
        m.tokens.emplace(p);
      }
    }

    // enqueue a noop to start, not
    // doing this would require
    // external input to 'start' (even if the petri net is alive)
    reducers.enqueue([](Model &&m) -> Model & { return m; });

    // this is the check whether we should break the loop. It's either early
    // exit, otherwise there must be event_logs.
    auto stop_condition = [&] {
      return EARLY_EXIT.load(std::memory_order_relaxed) ||
             (m.pending_transitions.empty() && !m.event_log.empty());
    };
    active.store(true);
    Reducer f;
    // get a reducer. Immediately, or wait a bit
    while (!stop_condition() &&
           reducers.wait_dequeue_timed(f, std::chrono::seconds(9000))) {
      do {
        m = f(std::move(m));
      } while (reducers.try_dequeue(f));
      blockIfPaused(case_id);
      m = runAll(m, reducers, stp, case_id);
      if (final_marking.has_value() &&
          MarkingReached(m.M, final_marking.value())) {
        break;
      }
    }
    active.store(false);

    // determine what was the reason we terminated.
    TransitionState result;
    if (EARLY_EXIT.load()) {
      result = TransitionState::UserExit;
    } else if (final_marking.has_value()
                   ? MarkingReached(m.M, final_marking.value())
                   : false) {
      result = TransitionState::Completed;
    } else if (m.pending_transitions.empty()) {
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
    const Store &store, const std::string &case_id,
    const symmetri::StoppablePool &stp) {
  const auto &[net, m0] = readPetriNets(files);
  createApplication(net, m0, final_marking, store, case_id, stp);
}

Application::Application(
    const symmetri::StateNet &net, const symmetri::NetMarking &m0,
    const std::optional<symmetri::NetMarking> &final_marking,
    const Store &store, const std::string &case_id,
    const symmetri::StoppablePool &stp) {
  createApplication(net, m0, final_marking, store, case_id, stp);
}

void Application::createApplication(
    const symmetri::StateNet &net, const symmetri::NetMarking &m0,
    const std::optional<symmetri::NetMarking> &final_marking,
    const Store &store, const std::string &case_id,
    const symmetri::StoppablePool &stp) {
  signal(SIGINT, signal_handler);
  std::stringstream s;
  s << "[%Y-%m-%d %H:%M:%S.%f] [%^%l%$] [thread %t] [" << case_id << "] %v";
  auto console = spdlog::stdout_color_mt(case_id);

  console->set_pattern(s.str());
  if (check(store, net)) {
    // init
    impl = std::make_shared<Impl>(net, m0, stp, final_marking, store, case_id);
    // register a function that "forces" transitions into the queue.
    p = [this](const std::string &t) {
      if (impl->active.load()) {
        impl->stp.enqueue([t, this, task = getTransition(impl->m.store, t)] {
          impl->reducers.enqueue(runTransition(t, task, impl->case_id));
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

std::tuple<clock_s::time_point, symmetri::Eventlog, symmetri::StateNet,
           symmetri::NetMarking, std::set<std::string>>
Application::get() const noexcept {
  auto &m = impl->getModel();

  return std::make_tuple(m.timestamp, impl->getEventLog(), m.net, m.M,
                         m.pending_transitions);
}

const symmetri::StateNet &Application::getNet() {
  const auto &m = impl->getModel();
  return m.net;
}

void Application::doMeData(std::function<void()> f) const {
  impl->reducers.enqueue([=](Model &&m) -> Model & {
    f();
    return m;
  });
}

void Application::togglePause() {
  std::lock_guard<std::mutex> lk(cv_m);
  PAUSE.store(!PAUSE.load());
  cv.notify_all();
}

}  // namespace symmetri
