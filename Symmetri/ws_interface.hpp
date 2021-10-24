#pragma once
#include "json.hpp"
#include <seasocks/PrintfLogger.h>
#include <seasocks/Server.h>

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
