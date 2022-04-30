#include "Symmetri/symmetri.h"

#include <blockingconcurrentqueue.h>
#include <signal.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include <fstream>
#include <functional>
#include <memory>
#include <numeric>
#include <optional>
#include <tuple>

#include "actions.h"
#include "model.h"
#include "pnml_parser.h"
#include "types.h"
#include "ws_interface.hpp"
auto getThreadId() {
  return static_cast<size_t>(
      std::hash<std::thread::id>()(std::this_thread::get_id()));
}
// Define the function to be called when ctrl-c (SIGINT) is sent to process

namespace {
bool EXIT = false;
std::function<void(int)> sighandler = [](int signal) { EXIT = true; };
inline void signal_handler(int signal) { sighandler(signal); }
}  // namespace

namespace symmetri {
using namespace moodycamel;

size_t calculateTrace(std::vector<Event> event_log) {
  // calculate a trace-id, in a simple way.
  std::sort(event_log.begin(), event_log.end(),
            [](const auto &lhs, const auto &rhs) {
              return std::get<clock_t::time_point>(lhs) <
                     std::get<clock_t::time_point>(rhs);
            });

  auto trace_hash = std::hash<std::string>{}(
      std::accumulate(event_log.begin(), event_log.end(), std::string(""),
                      [](const auto &acc, const Event &n) {
                        return std::get<2>(n) == TransitionState::Completed
                                   ? acc + std::get<1>(n)
                                   : acc;
                      }));
  return trace_hash;
}

constexpr auto noop = [](Model &&m) -> Model & { return m; };

bool check(const TransitionActionMap &store, const symmetri::StateNet &net) {
  return std::all_of(net.cbegin(), net.cend(), [&store](const auto &p) {
    const auto &t = std::get<0>(p);
    bool store_has_transition = store.contains(t);
    if (!store_has_transition) {
      spdlog::error("Transition {0} is not in store", t);
    }

    return store_has_transition;
  });
}

Application::Application(const std::set<std::string> &files,
                         const TransitionActionMap &store,
                         const std::string &case_id, bool interface) {
  const auto &[net, m0] = constructTransitionMutationMatrices(files);

  signal(SIGINT, signal_handler);
  std::stringstream s;
  s << "[%Y-%m-%d %H:%M:%S.%f] [%^%l%$] [thread %t] [" << case_id << "] %v";
  auto console = spdlog::stdout_color_mt(case_id);

  console->set_pattern(s.str());
  if (!check(store, net)) {
    spdlog::get(case_id)->error("Error not all transitions are in the store");
  }
  runApp = [=, this]() -> std::vector<Event> {
    auto server =
        interface ? std::optional(WsServer::Instance()) : std::nullopt;

    BlockingConcurrentQueue<Reducer> reducers(256);
    BlockingConcurrentQueue<object_t> polymorphic_actions(256);

    // register a function that "forces" transitions into the queue.
    p = [&a = polymorphic_actions, &store](const std::string &t) {
      a.enqueue(store.at(t));
    };

    // register the signal handler.
    sighandler = [&](int signal) {
      EXIT = true;
      reducers.enqueue(noop);
    };

    // returns a simple stoppable threadpool.
    auto stp = executeTransition(polymorphic_actions, 3, case_id);

    // the initial state
    auto m = Model(clock_t::now(), net, m0);

    // auto start
    reducers.enqueue(noop);

    Reducer f;
    Transitions T;
    // std::vector<object_t> T;

    while (true) {
      // get a reducer.
      while ((m.pending_transitions.empty() && m.M != m0)
                 ? reducers.try_dequeue(f)
                 : reducers.wait_dequeue_timed(f, std::chrono::seconds(1))) {
        if (EXIT) {
          break;
        }
        try {
          std::tie(m, T) = run_all(f(std::move(m)));
          for (const auto &t : T) {
            polymorphic_actions.enqueue([&, t] {
              const auto thread_id = getThreadId();
              const auto start_time = clock_t::now();
              run(store.at(t));
              const auto end_time = clock_t::now();

              reducers.enqueue(Reducer([=](Model &&model) -> Model & {
                for (const auto &m_p : model.net.at(t).second) {
                  model.M[m_p] += 1;
                }
                model.event_log.push_back(
                    {case_id, t, TransitionState::Started, start_time});
                model.event_log.push_back(
                    {case_id, t, TransitionState::Completed, end_time});

                model.pending_transitions.erase(t);
                model.transition_end_times[t] = end_time;
                model.log.emplace(
                    t, TaskInstance{start_time, end_time, thread_id});
                return model;
              }));
            });
          }
        } catch (const std::exception &e) {
          spdlog::get(case_id)->error(e.what());
        }
      }

      // server stuffies
      if (server.has_value()) {
        server.value()->sendNet(m.timestamp, m.net, m.M, m.pending_transitions,
                                m.transition_end_times);
        server.value()->sendLog(m.log);
      };
      m.log.clear();

      // end critiria. If there are no active transitions anymore.
      if ((m.pending_transitions.empty() && m.M != m0) || EXIT) {
        break;
      }
    };

    // publish a log
    spdlog::get(case_id)->info(
        std::string(EXIT ? "Forced shutdown" : "Deadlock") +
            " of {0}-net. End trace is {1}",
        case_id, calculateTrace(m.event_log));

    // stop the thread pool
    stp.stop();
    // if we have a viz server, kill it.
    if (server.has_value()) {
      server.value()->stop();
      spdlog::get(case_id)->info("Server stopped.");
    }

    return m.event_log;
  };
}

std::vector<Event> Application::operator()() const { return runApp(); };

}  // namespace symmetri
