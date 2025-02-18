#include "websocket_manager.h"
#include <iostream>

WebSocketManager::WebSocketManager(const std::string& server_address, unsigned short server_port) {
    server = std::make_shared<WebSocketServer>(server_address, server_port);
    client = std::make_unique<WebSocketClient>();
    setupLocalServer();
    setupDeribitClient();
}

WebSocketManager::~WebSocketManager() {
    stop();
}

void WebSocketManager::setupLocalServer() {
    server->onConnect([](std::shared_ptr<WebSocketSession> session) {
        std::cout << "New client connected to local server" << std::endl;
    });

    server->onMessage([this](std::shared_ptr<WebSocketSession> session, const std::string& message) {
        try {
            json j = json::parse(message);
            std::cout << "Client request received: " << j.dump(2) << std::endl;
            
            if (j.contains("subscribe") && client) {
                client->sendMessage(j.dump());
            }
        } catch (const json::parse_error& e) {
            std::cout << "Invalid message from client: " << message << std::endl;
        }
    });

    server->onDisconnect([](std::shared_ptr<WebSocketSession> session) {
        std::cout << "Client disconnected from local server" << std::endl;
    });
}

void WebSocketManager::setupDeribitClient() {
    client->onOpen([this]() {
        std::cout << "\nDeribit WebSocket connected!" << std::endl;
        connected = true;

        try {
            json subscribeMsg = {
                {"method", "public/subscribe"},
                {"params", {
                    {"channels", {"incremental_ticker.BNB_USDC"}}
                }},
                {"jsonrpc", "2.0"},
                {"id", 4}
            };
            
            std::cout << "Sending subscription message..." << std::endl;
            client->sendMessage(subscribeMsg.dump(4));
        } catch (const std::exception& e) {
            std::cerr << "Error sending subscription: " << e.what() << std::endl;
        }
    });

    client->onMessage([this](const std::string& message) {
        try {
            json j = json::parse(message);
            std::cout << "Received from Deribit: " << j.dump(2) << std::endl;
            
            // Broadcast to all connected clients
            if (server && j.contains("params") && j["params"].contains("data")) {
                server->broadcast(j.dump(2));
            }
        } catch (const json::parse_error& e) {
            std::cout << "Raw message from Deribit: " << message << std::endl;
        }
    });

    client->onClose([this]() {
        std::cout << "Deribit connection closed" << std::endl;
        connected = false;
        running = false;
    });

    client->onError([](const std::string& error) {
        if (error.find("Operation canceled") == std::string::npos &&
            error.find("stream truncated") == std::string::npos &&
            error.find("End of file") == std::string::npos) {
            std::cerr << "Deribit WebSocket error: " << error << std::endl;
        }
    });
}

void WebSocketManager::start() {
    std::cout << "Starting local WebSocket server on port(8000)..." << std::endl;
    server->run();
}

void WebSocketManager::stop() {
    if (client && connected) {
        std::cout << "Closing Deribit connection..." << std::endl;
        client->close();
    }

    if (server) {
        std::cout << "Stopping WebSocket server..." << std::endl;
        server->stop();
    }

    running = false;
}

void WebSocketManager::connectToDeribit(const std::string& host, const std::string& port, const std::string& path) {
    client->connect(host, port, path);
}

void WebSocketManager::sendToDeribit(const std::string& message) {
    if (client && connected) {
        client->sendMessage(message);
    }
}