#pragma once

/** @file executor.h */

#include <functional>

namespace symmetri {

/**
 * @brief Where transition callback bodies run. The Petri executor itself is a
 * single-threaded event loop that owns the marking and serializes all
 * mutations through its reducer queue; an Executor only decides where the
 * (potentially blocking) callback bodies execute. post() must run the task
 * exactly once, eventually. Tasks are self-contained and report back through
 * the net's reducer queue, so they may run on any thread — or inline on the
 * caller.
 *
 * The surface is deliberately minimal so adapters are trivial and symmetri
 * needs no dependency on any particular runtime. A ROS-compatible executor,
 * for example, is written entirely user-side:
 *
 *   struct RosExecutor final : symmetri::Executor {
 *     ros::CallbackQueue& q;
 *     void post(std::function<void()> task) override {
 *       // wrap the task in a ros::CallbackInterface and q.addCallback(...);
 *       // whatever spins that queue now runs the transition bodies.
 *     }
 *   };
 *
 * (equivalently for rclcpp: post into a node's executor/timer callback).
 */
struct Executor {
  virtual ~Executor() = default;

  /**
   * @brief Schedule a task for execution, exactly once.
   *
   * @param task a self-contained unit of work (a transition callback body)
   */
  virtual void post(std::function<void()> task) = 0;
};

/**
 * @brief Runs every task immediately on the calling thread — which, for
 * transition bodies, is the petri loop itself. This serializes the callback
 * bodies with the net: execution is fully deterministic and single-threaded,
 * which is ideal for tests, replay and simulation. The trade-offs are that a
 * blocking callback blocks the whole net, and cancel/pause cannot preempt a
 * body that is already running.
 */
struct InlineExecutor final : public Executor {
  void post(std::function<void()> task) override { task(); }
};

}  // namespace symmetri
