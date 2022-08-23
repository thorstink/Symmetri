#include "Symmetri/ws_interface.h"

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

WsServer::WsServer(int port)
    : time_data(std::make_shared<Handler>()),
      marking_transition(std::make_shared<Handler>()),
      server(std::make_shared<seasocks::PrintfLogger>(
          seasocks::Logger::Level::Error)),
      web_t_([this, port] {
        server.addWebSocketHandler("/transition_data", time_data);
        server.addWebSocketHandler("/marking_transition_data",
                                   marking_transition);
        server.startListening(port);
        server.setStaticPath("web");
        spdlog::info("interface online at http://localhost:{0}/", port);
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
    const immer::set<symmetri::Transition> &pending_transitions) {
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
