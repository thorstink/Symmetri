#pragma once

#include <thread>
#include <vector>
#include <atomic>
#include "symmetri/polyaction.h"

namespace symmetri {

class StoppablePool {
 private:
  void loop();
  std::vector<std::thread> pool;
  std::atomic<bool> stop_flag;

 public:
  StoppablePool(unsigned int thread_count);
  ~StoppablePool();
  void enqueue(PolyAction&& p) const;
  void stop();
};

}  // namespace symmetri
