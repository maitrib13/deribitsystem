#ifndef WEBSOCKET_MANAGER_H
#define WEBSOCKET_MANAGER_H

#include "websocket_client.h"
#include "websocket_server.h"
#include "order_placement.h"
#include <memory>
#include <atomic>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class WebSocketManager {
public:
    WebSocketManager(const std::string& server_address, unsigned short server_port);
    ~WebSocketManager();

    void start();
    void stop();
    bool isRunning() const { return running; }
    bool isConnected() const { return connected; }

    // Deribit client operations
    void connectToDeribit(const std::string& host, const std::string& port, const std::string& path);
    void sendToDeribit(const std::string& message);

private:
    std::atomic<bool> running{true};
    std::atomic<bool> connected{false};
    std::shared_ptr<WebSocketServer> server;
    std::unique_ptr<WebSocketClient> client;
    OrderPlacement orderHandler;

    void setupDeribitClient();
    void setupLocalServer();
};

#endif 