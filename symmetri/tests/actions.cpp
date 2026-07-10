#include "doctest/doctest.h"
#include "symmetri/tasks.h"

using namespace symmetri;
TEST_CASE("Run the executor") {
  // Create a simple task system with 1 thread.
  auto threadpool = std::make_shared<TaskSystem>(1);
  // launch a task
  std::atomic<bool> ran(false);
  threadpool->push([&]() { ran.store(true); });
  // wait a little before shutting the thread pool down.
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  CHECK(ran);
}

TEST_CASE("Run the executor parallel tasks") {
  // Create a simple task system with 2 threads.
  auto threadpool = std::make_shared<TaskSystem>(2);

  // launch a task
  std::atomic<bool> ran1(false);
  std::atomic<bool> ran2(false);
  std::thread::id thread_id1, thread_id2;
  const std::thread::id main_thread(std::this_thread::get_id());

  threadpool->push([&]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    thread_id1 = std::this_thread::get_id();
    ran1.store(true);
  });
  threadpool->push([&]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    thread_id2 = std::this_thread::get_id();
    ran2.store(true);
  });

  // block until both tasks finished
  while (!ran1.load() || !ran2.load()) {
  }
  CHECK((ran1.load() && ran2.load()));
  CHECK(static_cast<bool>(main_thread != thread_id1));
  CHECK(static_cast<bool>(main_thread != thread_id2));
}

TEST_CASE("InlineExecutor runs tasks immediately on the calling thread") {
  auto executor = std::make_shared<InlineExecutor>();
  bool ran = false;
  std::thread::id task_thread;
  executor->post([&] {
    ran = true;
    task_thread = std::this_thread::get_id();
  });
  // No waiting needed: post() returned means the task completed.
  CHECK(ran);
  CHECK(static_cast<bool>(task_thread == std::this_thread::get_id()));
}

TEST_CASE("TaskSystem satisfies the Executor interface") {
  // The pool is usable through the Executor base: this is the seam where a
  // user-provided executor (e.g. one posting into a ROS callback queue) plugs
  // in without symmetri knowing about it.
  std::shared_ptr<Executor> executor = std::make_shared<TaskSystem>(1);
  std::atomic<bool> ran(false);
  executor->post([&] { ran.store(true); });
  while (!ran.load()) {
  }
  CHECK(ran);
}
