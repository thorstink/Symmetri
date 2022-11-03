#pragma once

#include <seasocks/Server.h>

#include <thread>

#include "symmetri/types.h"
struct Handler;
class WsServer {
 public:
  WsServer(
      int port = 2222, std::function<void()> pause = []() {},
      const std::string &state_path = "share/web");
  ~WsServer();
  void stop();
  void sendNet(symmetri::clock_s::time_point now, const symmetri::StateNet &net,
               const symmetri::NetMarking &M,
               const std::set<symmetri::Transition> &pending_transitions);
  void sendLog(const symmetri::Eventlog &el);

 private:
  std::shared_ptr<Handler> time_data;
  std::shared_ptr<Handler> marking_transition;
  seasocks::Server server;
  std::thread web_t_;
};
