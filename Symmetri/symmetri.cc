// #include "actions.h"
#include "model.h"
#include "pnml_parser.h"
#include "symmetri.h"
#include "types.h"
#include <functional>
#include "actions.h"

namespace symmetri {
using namespace model;
using namespace actions;
using namespace moodycamel;

constexpr auto noop = [](const model::Model &m) { return m; };

std::function<void()> start(const std::string &pnml_path,
                            const model::TransitionActionMap &store) {

  return [=]() {
    const auto &[Dm, Dp, M0] = constructTransitionMutationMatrices(pnml_path);

    BlockingConcurrentQueue<Reducer> reducers(256);
    BlockingConcurrentQueue<types::Transition> actions(1024);

    auto tp = executeTransition(store, reducers, actions);
    auto m = Model(model::clock_t::now(), M0, Dm, Dp);

    // start 
    reducers.enqueue(noop);

    while (true) {
      Reducer f = noop;
      reducers.wait_dequeue(f);
      try {
        // run the reducer
        auto [r, transitions] = run(f(m));
        m = std::move(r);
        // dispatch the transitions
        for (auto transition : transitions)
          actions.enqueue(transition);

        m.data->timestamp = model::clock_t::now();
      } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
      }
    };
  };
}
} // namespace symmetri
