#include "model.h"

#include <iostream>
namespace symmetri {

auto getThreadId() {
  return static_cast<size_t>(
      std::hash<std::thread::id>()(std::this_thread::get_id()));
}

PolyAction getTransition(const Store &s, const std::string &t) {
  return std::find_if(s.begin(), s.end(),
                      [&](const auto &t_i) { return t == t_i.first; })
      ->second;
}

void processPreConditions(const std::vector<Place> &pre_places, NetMarking &m) {
  for (const auto &m_p : pre_places) {
    m[m_p] -= 1;
  }
}

void processPostConditions(const std::vector<Place> &post_places,
                           NetMarking &m) {
  for (const auto &m_p : post_places) {
    m[m_p] += 1;
  }
}

Reducer processTransition(const std::string &T_i, const std::string &case_id,
                          TransitionState result, size_t thread_id,
                          clock_s::time_point start_time,
                          clock_s::time_point end_time) {
  return [=](Model &&model) -> Model & {
    if (result == TransitionState::Completed) {
      const auto &post = model.net.at(T_i).second;
      model.tokens.insert(post.begin(), post.end());
      processPostConditions(post, model.M);
    }
    model.event_log.push_back(
        {case_id, T_i, TransitionState::Started, start_time, thread_id});
    model.event_log.push_back({case_id, T_i,
                               result == TransitionState::Completed
                                   ? TransitionState::Completed
                                   : TransitionState::Error,
                               end_time, thread_id});
    model.pending_transitions.erase(T_i);
    return model;
  };
}

Reducer processTransition(const std::string &T_i, const Eventlog &new_events,
                          TransitionState result, size_t thread_id,
                          clock_s::time_point end_time) {
  return [=](Model &&model) -> Model & {
    if (result == TransitionState::Completed) {
      const auto &post = model.net.at(T_i).second;
      model.tokens.insert(post.begin(), post.end());
      processPostConditions(post, model.M);
    }

    for (const auto &e : new_events) {
      model.event_log.push_back(e);
    }
    model.pending_transitions.erase(T_i);
    return model;
  };
}

Reducer runTransition(const std::string &T_i, const PolyAction &task,
                      const std::string &case_id,
                      const clock_s::time_point &queue_time) {
  const auto start_time = clock_s::now();
  const auto &[ev, res] = runTransition(task);
  const auto end_time = clock_s::now();
  const auto thread_id = getThreadId();
  return ev.empty() ? processTransition(T_i, case_id, res, thread_id,
                                        start_time, end_time)
                    : processTransition(T_i, ev, res, thread_id, end_time);
}

Model &runAll(Model &model,
              moodycamel::BlockingConcurrentQueue<Reducer> &reducers,
              const StoppablePool &polymorphic_actions,
              const std::string &case_id) {
  model.timestamp = clock_s::now();
  // otherwise calculate the possible transitions.
  auto n = 0;
  while (n != model.tokens.size()) {
    n = model.tokens.size();
    for (const auto &place : model.tokens) {
      const auto ptr = std::find_if(
          std::begin(model.reverse_loopup), std::end(model.reverse_loopup),
          [&place](const auto &u) { return u.first == place; });
      if (ptr == model.reverse_loopup.end() || ptr->second.empty()) {
        continue;
      }
      const auto &ts = ptr->second;
      for (const auto &T_i : ts) {
        const auto &pre = model.net.at(T_i).first;
        if (std::all_of(std::begin(pre), std::end(pre), [&](const auto &m_p) {
              return std::count(std::begin(model.tokens),
                                std::end(model.tokens), m_p) >=
                     std::count(std::begin(pre), std::end(pre), m_p);
            })) {
          // deduct the marking
          for (const auto &p_ : pre) {
            model.tokens.extract(p_);
          }
          processPreConditions(pre, model.M);

          // if the function is nullopt_t, we short-circuit the marking
          // mutation and do it immediately.
          const auto &task =
              std::find_if(model.store.begin(), model.store.end(),
                           [&](const auto &u) { return u.first == T_i; })
                  ->second;
          if constexpr (std::is_same_v<std::nullopt_t, decltype(task)>) {
            const auto &post = model.net.at(T_i).second;
            for (const auto &p_ : post) {
              model.tokens.emplace(p_);
            }
            processPostConditions(post, model.M);
          } else {
            polymorphic_actions.enqueue([&reducers, T_i, task, case_id,
                                         queue_time = clock_s::now()] {
              reducers.enqueue(runTransition(T_i, task, case_id, queue_time));
            });
            model.pending_transitions.insert(T_i);
          }
          break;  // break because we changed the marking.
        }
      }
      if (model.tokens.size() != n) {
        break;  // break because we changed the marking.
      }
    }
  }
  return model;
}
}  // namespace symmetri
