#pragma once
#include <blockingconcurrentqueue.h>

#include <thread>
#include <vector>

#include "model.h"

namespace symmetri {

std::vector<std::thread> executeTransition(
    const TransitionActionMap &local_store, const Conversions &marking_mapper,
    moodycamel::BlockingConcurrentQueue<Reducer> &reducers,
    moodycamel::BlockingConcurrentQueue<Transition> &actions, int state_size,
    unsigned int thread_count);

}  // namespace symmetri
