#include "Symmetri/retry.h"

#include <spdlog/spdlog.h>

symmetri::PolyAction symmetri::retryFunc(const symmetri::PolyAction &f,
                                         const symmetri::Transition &t,
                                         const std::string &case_id,
                                         unsigned int retry_count) {
  return [=]() -> std::pair<symmetri::Eventlog, symmetri::TransitionState> {
    symmetri::Eventlog log;
    symmetri::TransitionState res;
    unsigned int attempts = 0;
    do {
      attempts++;
      const auto thread_id = static_cast<size_t>(
          std::hash<std::thread::id>()(std::this_thread::get_id()));
      symmetri::Eventlog incremental_log(2);
      const auto start_time = symmetri::clock_s::now();
      std::tie(incremental_log, res) = runTransition(f);
      const auto end_time = symmetri::clock_s::now();
      if (incremental_log.empty()) {
        incremental_log = incremental_log.push_back(
            {case_id, t, symmetri::TransitionState::Started, start_time,
             thread_id});
        incremental_log =
            incremental_log.push_back({case_id, t, res, end_time, thread_id});
      }
      log = log + incremental_log;

    } while (symmetri::TransitionState::Completed != res &&
             attempts < retry_count);

    // publish a
    // if (attempts > 1) {
    //   spdlog::get(case_id)->info(
    //       printState(res) + " after {0} retries of {2}. Trace-hash is {1}",
    //       attempts, calculateTrace(log), t);
    // }
    return {log, res};
  };
}
