#pragma once
#include <blockingconcurrentqueue.h>

#include <thread>
#include <vector>

#include "model.h"

namespace symmetri {
struct RetType {
  std::vector<std::thread> pool;
  std::shared_ptr<std::atomic<bool>> stop = std::make_shared<std::atomic<bool>>(false);
};

RetType executeTransition(
    const TransitionActionMap &local_store, const Conversions &marking_mapper,
    moodycamel::BlockingConcurrentQueue<Reducer> &reducers,
    moodycamel::BlockingConcurrentQueue<Transition> &actions, int state_size,
    unsigned int thread_count, const std::string &case_id);

}  // namespace symmetri
