
#include "petri.h"
namespace symmetri {

size_t toIndex(const std::vector<std::string> &m, const std::string &s) {
  auto ptr = std::find(m.begin(), m.end(), s);
  return std::distance(m.begin(), ptr);
}

bool canFire(const SmallVector &pre, const std::vector<size_t> &tokens) {
  return std::all_of(pre.begin(), pre.end(), [&](const auto &m_p) {
    return std::count(tokens.begin(), tokens.end(), m_p) >=
           std::count(pre.begin(), pre.end(), m_p);
  });
}

gch::small_vector<size_t, 32> possibleTransitions(
    const std::vector<size_t> &tokens,
    const std::vector<SmallVector> &p_to_ts_n) {
  gch::small_vector<size_t, 32> possible_transition_list_n;
  for (const size_t place : tokens) {
    for (size_t t : p_to_ts_n[place]) {
      if (std::find(possible_transition_list_n.begin(),
                    possible_transition_list_n.end(),
                    t) == possible_transition_list_n.end()) {
        possible_transition_list_n.push_back(t);
      }
    }
  }

  return possible_transition_list_n;
}

Reducer createReducerForCallback(const size_t t_i, const Result result,
                                 const Clock::time_point t_end) {
  return [=](Petri &model) {
    // if it is in the active transition set it means it is finished and we
    // should process it.
    auto it = std::find(model.scheduled_callbacks.begin(),
                        model.scheduled_callbacks.end(), t_i);
    if (it != model.scheduled_callbacks.end()) {
      model.scheduled_callbacks.erase(it);
      if (result == state::Completed) {
        const auto &place_list = model.net.output_n;
        model.tokens.insert(model.tokens.begin(), place_list[t_i].begin(),
                            place_list[t_i].end());
      }
    };
    // get the log
    Eventlog ev;
    model.net.store[t_i](ev);
    const Transition &transition = model.net.transition[t_i];
    if (!ev.empty()) {
      model.event_log.insert(model.event_log.end(), ev.begin(), ev.end());
    }
    model.event_log.push_back({model.case_id, transition, result, t_end});
  };
}

Reducer scheduleCallback(
    size_t t_i, const Callback &task,
    const std::shared_ptr<moodycamel::BlockingConcurrentQueue<Reducer>>
        &reducer_queue) {
  reducer_queue->enqueue(
      [start_time = Clock::now(), t_i](symmetri::Petri &model) {
        model.event_log.push_back({model.case_id, model.net.transition[t_i],
                                   state::Started, start_time});
      });

  return createReducerForCallback(t_i, ::fire(task), Clock::now());
}

}  // namespace symmetri
