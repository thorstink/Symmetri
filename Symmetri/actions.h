#pragma once
#include "model.h"
#include <blockingconcurrentqueue.h>
#include <thread>
#include <vector>

namespace symmetri {

std::vector<std::thread>
executeTransition(const TransitionActionMap &local_store,
                  moodycamel::BlockingConcurrentQueue<Reducer> &reducers,
                  moodycamel::BlockingConcurrentQueue<Transition> &actions,
                  unsigned int thread_count);

} // namespace symmetri
