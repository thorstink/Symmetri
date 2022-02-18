#include "Symmetri/symmetri.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <fstream>
#include <functional>
#include <memory>
#include <tuple>

#include "Symmetri/types.h"
#include "actions.h"
#include "model.h"
#include "pnml_parser.h"
#include "ws_interface.hpp"
namespace symmetri {
using namespace moodycamel;

constexpr auto noop = [](const Model &m) { return m; };

std::function<symmetri::OptionalError()> start(
    const std::set<std::string> &files, const TransitionActionMap &store,
    const std::string &case_id, bool interface) {
  const auto &[Dm, Dp, M0, json_net, transitions, places] =
      constructTransitionMutationMatrices(files);

  std::stringstream s;
  s << "[%Y-%m-%d %H:%M:%S.%f] [%^%l%$] [thread %t] [" << case_id << "] %v";
  auto console = spdlog::stdout_color_mt(case_id);

  console->set_pattern(s.str());

  return [=]() {
    // auto server = WsServer::Instance(json_net);
    auto server =
        interface ? std::optional(WsServer::Instance(json_net)) : std::nullopt;
    BlockingConcurrentQueue<Reducer> reducers(256);
    BlockingConcurrentQueue<Transition> actions(1024);
    auto tp = executeTransition(store, places, reducers, actions, M0.size(), 3,
                                case_id);
    auto m = Model(clock_t::now(), M0, Dm, Dp, actions);

    // auto start
    reducers.enqueue(noop);

    Reducer f = noop;
    while (true) {
      // get a reducer.
      while (reducers.wait_dequeue_timed(f, std::chrono::seconds(1))) {
        m.data->timestamp = clock_t::now();
        try {
          m = run_all(f(m));
        } catch (const std::exception &e) {
          spdlog::get(case_id)->error(e.what());
        }
      }

      auto data = (*m.data);
      m.data->log.clear();
      std::stringstream log_data;
      for (const auto &[a, b, c, d] : data.log) {
        log_data << a << ',' << b << ',' << c << ',' << d << '\n';
      }

      nlohmann::json j;
      j["active_transitions"] = nlohmann::json(m.data->active_transitions);
      j["marking"] = webMarking(m.data->M, places.id_to_label_);

      if (server.has_value()) {
        server.value()->queueTask(
            [=]() { server.value()->marking_transition->send(j.dump()); });
        server.value()->queueTask(
            [=, l = log_data.str()]() { server.value()->time_data->send(l); });
      }

      if (m.data->active_transitions.size() == 0) {
        break;
      }
    };
    for (auto &&t : tp) {
      t.detach();
    }
    return std::nullopt;
  };
}
}  // namespace symmetri
