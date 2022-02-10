#include "Symmetri/symmetri.h"

#include <fstream>
#include <functional>
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
    const std::set<std::string> &files, const TransitionActionMap &store) {
  const auto &[Dm, Dp, M0, json_net, transitions, places] =
      constructTransitionMutationMatrices(files);
  return [=]() {
    WsServer *server = WsServer::Instance(json_net);
    BlockingConcurrentQueue<Reducer> reducers(256);
    BlockingConcurrentQueue<Transition> actions(1024);

    auto tp = executeTransition(store, places, reducers, actions, M0.size(), 3);
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
          std::cerr << e.what() << '\n';
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

      server->queueTask([=]() { server->marking_transition->send(j.dump()); });
      server->queueTask(
          [=, l = log_data.str()]() { server->time_data->send(l); });

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
