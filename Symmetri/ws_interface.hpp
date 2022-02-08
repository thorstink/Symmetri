#pragma once
#include <blockingconcurrentqueue.h>
#include <seasocks/PrintfLogger.h>
#include <seasocks/Server.h>

#include <thread>

#include "json.hpp"
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
};

struct Wsio : seasocks::WebSocket::Handler {
  Wsio(const nlohmann::json &net) : net_(net) {}
  std::set<seasocks::WebSocket *> connections;
  const nlohmann::json net_;
  void onConnect(seasocks::WebSocket *socket) override {
    connections.insert(socket);
    for (auto *con : connections) {
      con->send(net_.dump());
    }
  }
  void onDisconnect(seasocks::WebSocket *socket) override {
    connections.erase(socket);
  }
  void send(const std::string &msg) {
    for (auto *con : connections) {
      con->send(msg);
    }
  }
};

class WsServer {
 public:
  std::shared_ptr<Output> time_data;
  std::shared_ptr<Wsio> marking_transition;
  static WsServer *Instance(const nlohmann::json &json_net);
  void queueTask(const std::function<void()> &task) { web.try_enqueue(task); }
  void stop() {
    runweb.store(false);
    web_t_.join();
  }

 private:
  WsServer(const nlohmann::json &json_net)
      : time_data(std::make_shared<Output>()),
        marking_transition(std::make_shared<Wsio>(json_net)),
        web(256),
        runweb(true),
        web_t_([this] {
          seasocks::Server server(std::make_shared<seasocks::PrintfLogger>(
              seasocks::Logger::Level::Error));
          server.addWebSocketHandler("/transition_data", time_data);
          server.addWebSocketHandler("/marking_transition_data",
                                     marking_transition);
          server.startListening(2222);
          server.setStaticPath("web");
          std::cout << "interface online at http://localhost:2222/"
                    << std::endl;

          std::function<void()> task;
          while (runweb.load()) {
            while (web.try_dequeue(task)) {
              task();
            }
            server.poll(10);
          }
          server.terminate();
        }) {}                    // Constructor is private.
  WsServer(WsServer const &) {}  // Copy constructor is private.
  WsServer &operator=(WsServer const &) {
    return *this;
  }  // Assignment operator is private.

  static WsServer *instance_;

  moodycamel::BlockingConcurrentQueue<std::function<void()>> web;
  std::atomic<bool> runweb;
  std::thread web_t_;
};

WsServer *WsServer::instance_ = NULL;

WsServer *WsServer::Instance(const nlohmann::json &json_net) {
  if (!instance_) {
    instance_ = new WsServer(json_net);
  }

  return instance_;
};
