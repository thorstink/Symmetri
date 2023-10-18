#include <algorithm>
#include <sstream>

#include "symmetri/parsers.h"

namespace symmetri {

std::string mermaidFromEventlog(symmetri::Eventlog el) {
  auto now_timestamp = Clock::now();
  el.erase(std::remove_if(el.begin(), el.end(),
                          [](const auto &x) {
                            return x.state == symmetri::Color::Scheduled;
                          }),
           el.end());
  std::sort(el.begin(), el.end(), [](const auto &a, const auto &b) {
    if (a.case_id != b.case_id) {
      return a.case_id < b.case_id;
    }
    if (a.stamp != b.stamp) {
      return a.stamp < b.stamp;
    }
    if (a.transition != b.transition) {
      return a.transition < b.transition;
    }
    return false;
  });

  if (el.empty()) {
    return "";
  }

  std::stringstream mermaid;
  mermaid << "\n---\ndisplayMode : compact\n---\ngantt\ntitle A Gantt "
             "Diagram\ndateFormat x\naxisFormat %H:%M:%S\n";
  std::string current_section("");
  for (auto it = el.begin(); std::next(it) != el.end(); it = std::next(it)) {
    const auto &start = *it;
    const auto &end = *std::next(it);
    auto id1 = start.case_id + start.transition;
    auto id2 = end.case_id + end.transition;
    if (start.state == symmetri::Color::Started) {
      if (current_section != start.case_id) {
        current_section = start.case_id;
        mermaid << "section " << start.case_id << "\n";
      }
      auto result = (end.state == symmetri::Color::Error ||
                     end.state == symmetri::Color::Deadlock)
                        ? "crit"
                        : (end.state == symmetri::Color::UserExit  // Paused
                               ? "active"
                               : "done");  // Success

      mermaid
          << start.transition << " :" << (id1 == id2 ? result : "active")
          << ", "
          << std::chrono::duration_cast<std::chrono::milliseconds>(
                 start.stamp.time_since_epoch())
                 .count()
          << ','
          << std::chrono::duration_cast<std::chrono::milliseconds>(
                 (id1 == id2 ? end.stamp : now_timestamp).time_since_epoch())
                 .count()
          << '\n';
    }
  }

  // and check if the latest is an active transition
  const auto &start = el.back();
  if (start.state == symmetri::Color::Started) {
    mermaid << start.transition << " :"
            << "active"
            << ", "
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   start.stamp.time_since_epoch())
                   .count()
            << ','
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   now_timestamp.time_since_epoch())
                   .count()
            << '\n';
  }

  return mermaid.str();
}
}  // namespace symmetri
