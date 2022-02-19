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

const std::string active = "active";
const std::string active_tag = ":::" + active;
const std::string active_def = "classDef " + active + " fill:#f96;\n";
const std::string passive = "passive";
const std::string passive_tag = ":::" + passive;
const std::string passive_def = "classDef " + passive + " fill:#c4c;\n";
const std::string conn = "---";
const std::string header = "graph TB\n" + active_def + passive_def;

std::string placeFormatter(const std::string &id, int marking = 0) {
  return id + "((" + id + " : " + std::to_string(marking) + "))";
}

auto getPlaceMarking(
    const std::map<Eigen::Index, std::string> &index_marking_map,
    const Marking &marking) {
  std::map<std::string, int> id_marking_map;
  for (const auto &[idx, id] : index_marking_map) {
    id_marking_map.emplace(id, marking.coeff(idx));
  }
  return id_marking_map;
}

auto genNet(const nlohmann::json &j,
            const std::map<std::string, int> &id_marking_map) {
  std::stringstream mermaid;
  mermaid << header;
  for (const auto &x : j["transitions"]) {
    for (const auto &post : x["post"].items()) {
      int marking = id_marking_map.at(post.key());

      mermaid << x["label"].get<std::string>() + passive_tag << conn
              << placeFormatter(post.key(), marking) << "\n";
    }
    for (const auto &pre : x["pre"].items()) {
      int marking = id_marking_map.at(pre.key());

      mermaid << placeFormatter(pre.key(), marking) << conn
              << x["label"].get<std::string>() + passive_tag << "\n";
    }
  }

  return mermaid.str();
}

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
    auto server =
        interface ? std::optional(WsServer::Instance(genNet(json_net, getPlaceMarking(places.id_to_label_, M0))))
                  : std::nullopt;
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

      for (auto &&[task_id, task_instance] : data.log) {
        auto &&[start, end, thread_id] = task_instance;
        log_data << thread_id << ','
                 << std::chrono::duration_cast<std::chrono::milliseconds>(
                        start.time_since_epoch())
                        .count()
                 << ','
                 << std::chrono::duration_cast<std::chrono::milliseconds>(
                        end.value_or(m.data->timestamp).time_since_epoch())
                        .count()
                 << ',' << task_id << '\n';
      }
      auto net =
          genNet(json_net, getPlaceMarking(places.id_to_label_, m.data->M));
      spdlog::info(net);

      nlohmann::json j;
      j["active_transitions"] = nlohmann::json(m.data->active_transitions);

      if (server.has_value()) {
        server.value()->queueTask(
            [=]() { server.value()->marking_transition->send(net); });
        server.value()->queueTask(
            [=, l = log_data.str()]() { server.value()->time_data->send(l); });
      }

      if (m.data->active_transitions.size() == 0) {
        break;
      }
    };
    tp.stop();
    return std::nullopt;
  };
}
}  // namespace symmetri
