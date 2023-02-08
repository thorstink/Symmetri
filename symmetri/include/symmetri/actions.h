#pragma once

#include <atomic>
#include <thread>
#include <vector>

#include "symmetri/polyaction.h"

namespace symmetri {

class StoppablePool;

std::shared_ptr<const StoppablePool> createStoppablePool(
    unsigned int thread_count);

class StoppablePool {
 private:
  void loop();
  std::vector<std::thread> pool;
  std::atomic<bool> stop_flag;
  StoppablePool(const StoppablePool&) = delete;
  StoppablePool& operator=(const StoppablePool&) = delete;
  friend std::shared_ptr<const StoppablePool> createStoppablePool(
      unsigned int thread_count) {
    return std::shared_ptr<const StoppablePool>(
        new StoppablePool(thread_count));
  }

  StoppablePool(unsigned int thread_count);

 public:
  ~StoppablePool();
  void enqueue(PolyAction&& p) const;
};

}  // namespace symmetri
