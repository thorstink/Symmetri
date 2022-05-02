#pragma once

#include <seasocks/PrintfLogger.h>
#include <seasocks/Server.h>
#include <spdlog/spdlog.h>

#include <thread>

#include "mermaid.hpp"

using namespace seasocks;

struct Output : seasocks::WebSocket::Handler {
  std::set<seasocks::WebSocket *> connections;
  void onConnect(seasocks::WebSocket *socket) override {
    connections.insert(socket);
  }
  void onDisconnect(seasocks::WebSocket *socket) override {
    connections.erase(socket);
  }
  void send(const std::string &msg) {
    for (auto *con : connections) {
      con->send(msg);
    }
  }
  bool hasConnections() { return connections.size() > 0; }
};

struct Wsio : seasocks::WebSocket::Handler {
  Wsio() {}
  std::set<seasocks::WebSocket *> connections;
  void onConnect(seasocks::WebSocket *socket) override {
    connections.insert(socket);
  }
  void onDisconnect(seasocks::WebSocket *socket) override {
    connections.erase(socket);
  }
  void send(const std::string &msg) {
    for (auto *con : connections) {
      con->send(msg);
    }
  }
  bool hasConnections() { return connections.size() > 0; }
};

class WsServer {
 public:
  std::shared_ptr<Output> time_data;
  std::shared_ptr<Wsio> marking_transition;
  static std::shared_ptr<WsServer> Instance();
  void sendNet(symmetri::clock_t::time_point now, const symmetri::StateNet &net,
               symmetri::NetMarking M,
               std::set<symmetri::Transition> pending_transitions,
               std::map<symmetri::Transition, symmetri::clock_t::time_point>
                   transition_end_times) {
    server->execute([=, this]() {
      marking_transition->send(symmetri::genNet(
          now, net, M, pending_transitions, transition_end_times));
    });
  }
  void sendLog(const std::string &log) {
    server->execute([log, this]() { time_data->send(log); });
  }
  void stop() {
    server->terminate();
    if (web_t_.joinable()) {
      web_t_.join();
    }
  }

 private:
  WsServer()
      : time_data(std::make_shared<Output>()),
        marking_transition(std::make_shared<Wsio>()),
        web_t_([this] {
          server = std::make_shared<seasocks::Server>(
              std::make_shared<seasocks::PrintfLogger>(
                  seasocks::Logger::Level::Error));
          server->addWebSocketHandler("/transition_data", time_data);
          server->addWebSocketHandler("/marking_transition_data",
                                      marking_transition);
          auto port = 2222;
          server->startListening(port);
          server->setStaticPath("web");
          spdlog::info("interface online at http://localhost:{0}/", port);
          server->loop();
        }) {}                    // Constructor is private.
  WsServer(WsServer const &) {}  // Copy constructor is private.
  WsServer &operator=(WsServer const &) {
    return *this;
  }  // Assignment operator is private.

  static std::shared_ptr<WsServer> instance_;
  std::shared_ptr<seasocks::Server> server;
  std::atomic<bool> runweb;
  std::thread web_t_;
};

std::shared_ptr<WsServer> WsServer::instance_ = NULL;

std::shared_ptr<WsServer> WsServer::Instance() {
  if (!instance_) {
    instance_.reset(new WsServer());
  }

  return instance_;
};
