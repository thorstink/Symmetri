#include "symmetri/ws_interface.h"

#include <seasocks/PrintfLogger.h>
#include <spdlog/spdlog.h>

#include "mermaid.hpp"

struct Handler : seasocks::WebSocket::Handler {
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

struct pauseHandler : seasocks::WebSocket::Handler {
  std::function<void()> p_;
  pauseHandler(std::function<void()> p) : p_(p) {}
  void onData(seasocks::WebSocket *con, const char *data) override { p_(); }
  void onConnect(seasocks::WebSocket *socket) override {}
  void onDisconnect(seasocks::WebSocket *socket) override {}
};

WsServer::WsServer(int port, std::function<void()> pause,
                   const std::string &static_path)
    : time_data(std::make_shared<Handler>()),
      marking_transition(std::make_shared<Handler>()),
      server(std::make_shared<seasocks::PrintfLogger>(
          seasocks::Logger::Level::Error)),
      web_t_([this, static_path, port, pause] {
        server.addWebSocketHandler("/pause",
                                   std::make_shared<pauseHandler>(pause));
        server.addWebSocketHandler("/transition_data", time_data);
        server.addWebSocketHandler("/marking_transition_data",
                                   marking_transition);
        server.startListening(port);
        server.setStaticPath(static_path.c_str());
        spdlog::info(
            "interface online at http://localhost:{0}/, server files from {1}",
            port, static_path);
        server.loop();
      }) {}

WsServer::~WsServer() { stop(); }

void WsServer::stop() {
  server.terminate();
  if (web_t_.joinable()) {
    web_t_.join();
  }
}
void WsServer::sendNet(
    symmetri::clock_s::time_point now, const symmetri::StateNet &net,
    const symmetri::NetMarking &M,
    const std::set<symmetri::Transition> &pending_transitions) {
  server.execute([=, this]() {
    marking_transition->send(
        symmetri::genNet(now, net, M, pending_transitions));
  });
}
void WsServer::sendLog(const symmetri::Eventlog &el) {
  server.execute([log = symmetri::stringLogEventlog(el), this]() {
    time_data->send(log);
  });
}
