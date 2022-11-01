#pragma once

#include <thread>
#include <vector>

#include "Symmetri/polyaction.h"

namespace symmetri {

class StoppablePool {
 private:
  void join();
  void loop();
  std::vector<std::thread> pool;
  std::atomic<bool> stop_flag;

 public:
  StoppablePool(unsigned int thread_count);
  void enqueue(const PolyAction &p);
  void stop();
};

}  // namespace symmetri
