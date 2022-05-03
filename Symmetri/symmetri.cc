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
#include <ranges>
#include <tuple>

#include "actions.h"
#include "model.h"
#include "pnml_parser.h"
#include "ws_interface.hpp"

namespace symmetri {
// Define the function to be called when ctrl-c (SIGINT) is sent to process
bool EARLY_EXIT = false;
inline void signal_handler(int signal) { EARLY_EXIT = true; }

using namespace moodycamel;

size_t calculateTrace(std::vector<Event> event_log) {
  // calculate a trace-id, in a simple way.
  std::sort(
      event_log.begin(), event_log.end(),
      [](const auto &lhs, const auto &rhs) { return lhs.stamp < rhs.stamp; });

  return std::hash<std::string>{}(std::accumulate(
      event_log.begin(), event_log.end(), std::string(""),
      [](const auto &acc, const Event &n) {
        return n.state == TransitionState::Completed ? acc + n.transition : acc;
      }));
}

std::string printState(symmetri::TransitionState s) {
  return s == symmetri::TransitionState::Started     ? "Started"
         : s == symmetri::TransitionState::Completed ? "Completed"
                                                     : "Error";
}

Eventlog getNewEvents(const Eventlog &el, clock_t::time_point t) {
  Eventlog new_events;
  std::copy_if(el.begin(), el.end(), std::back_inserter(new_events),
               [&](const auto &l) { return l.stamp > t; });
  return new_events;
}

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
                         unsigned int thread_count, const std::string &case_id,
                         bool interface) {
  const auto &[net, m0] = constructTransitionMutationMatrices(files);

  signal(SIGINT, signal_handler);
  std::stringstream s;
  s << "[%Y-%m-%d %H:%M:%S.%f] [%^%l%$] [thread %t] [" << case_id << "] %v";
  auto console = spdlog::stdout_color_mt(case_id);

  console->set_pattern(s.str());

  runApp =
      !check(store, net)
          ? std::function([=, this]() -> TransitionResult {
              spdlog::get(case_id)->error(
                  "Error not all transitions are in the store");
              return {{}, TransitionState::Error};
            })
          : std::function([=, this]() -> TransitionResult {
              auto server = interface ? std::optional(WsServer::Instance())
                                      : std::nullopt;

              BlockingConcurrentQueue<Reducer> reducers(256);
              BlockingConcurrentQueue<PolyAction> polymorphic_actions(256);

              // register a function that "forces" transitions into the queue.
              p = [&](const std::string &t) {
                polymorphic_actions.enqueue([&, &task = store.at(t)] {
                  reducers.enqueue(runTransition(t, task, case_id));
                });
              };

              // returns a simple stoppable threadpool.
              StoppablePool stp(thread_count, polymorphic_actions);

              // the initial state
              auto m = Model(net, store, m0);

              // enqueue a noop to start, not doing this would require external
              // input to 'start' (even if the petri net is alive)
              reducers.enqueue([](Model &&m) -> Model & { return m; });

              Reducer f;
              do {
                auto old_stamp = m.timestamp;
                // get a reducer.
                while ((m.pending_transitions.empty() && m.M != m0)
                           ? reducers.try_dequeue(f)
                           : reducers.wait_dequeue_timed(
                                 f, std::chrono::seconds(1))) {
                  if (EARLY_EXIT) {
                    break;
                  }
                  try {
                    m = runAll(f(std::move(m)), reducers, polymorphic_actions,
                               case_id);
                  } catch (const std::exception &e) {
                    spdlog::get(case_id)->error(e.what());
                  }
                }

                // server stuffies
                if (server.has_value()) {
                  server.value()->sendNet(m.timestamp, m.net, m.M,
                                          m.pending_transitions,
                                          m.transition_end_times);
                  server.value()->sendLog(
                      stringLogEventlog(getNewEvents(m.event_log, old_stamp)));
                };

                // end critiria. If there are no active transitions anymore.
                if (m.pending_transitions.empty() && m.M != m0) {
                  break;
                }
              } while (!EARLY_EXIT);

              // This point is only reached if the petri net deadlocked or we
              // exited the application early.

              // publish a log
              spdlog::get(case_id)->info(
                  std::string(EARLY_EXIT ? "Forced shutdown" : "Deadlock") +
                      " of {0}-net. Trace-hash is {1}",
                  case_id, calculateTrace(m.event_log));

              // stop the thread pool, blocks.
              stp.stop();
              // if we have a viz server, kill it. blocks.
              if (server.has_value()) {
                server.value()->stop();
                spdlog::get(case_id)->info("Server stopped.");
              }

              // this could have nuance, if maybe an expected state was reached?
              return {m.event_log, EARLY_EXIT ? TransitionState::Error
                                              : TransitionState::Completed};
            });
}

TransitionResult Application::operator()() const { return runApp(); };

}  // namespace symmetri
