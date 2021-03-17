#pragma once
#include "model.hpp"
#include <iostream>
#include <rxcpp/rx.hpp>
#include <seasocks/PrintfLogger.h>
#include <seasocks/Server.h>

using namespace rxcpp::rxo;
using namespace seasocks;

struct Handler : WebSocket::Handler {
  Handler(rxcpp::subscriber<const char *> dest) : dest(dest){};
  rxcpp::subscriber<const char *> dest;
  std::set<WebSocket *> connections;
  void onConnect(WebSocket *socket) override { connections.insert(socket); }
  void onData(WebSocket *, const char *data) override { dest.on_next(data); }
  void onDisconnect(WebSocket *socket) override { connections.erase(socket); }
};

struct Output : WebSocket::Handler {
  std::set<WebSocket *> connections;
  void onConnect(WebSocket *socket) override { connections.insert(socket); }
  void onDisconnect(WebSocket *socket) override { connections.erase(socket); }
  void send(const std::string &msg) {
    for (auto *con : connections) {
      con->send(msg);
    }
  }
};

rxcpp::observable<model::Reducer> serverSource(seasocks::Server &server) {
  seasocks::Server *ptr;
  ptr = &server;
  auto ws = rxcpp::observable<>::create<const char *>(
      [ptr](rxcpp::subscriber<const char *> dest) {
        ptr->addWebSocketHandler("/ws", std::make_shared<Handler>(dest));
      });

  return ws.map([](const char *data) {
    return model::Reducer([data](model::Model model) {
      std::cout << "IM A STREAM: " << data << '\n';
      return model;
    });
  });
}
