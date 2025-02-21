#include "websocket_manager.h"
#include <iostream>

// Helper struct for subscription tracking (defined in cpp to keep header clean)
struct SubscriptionInfo {
    std::string type;     // "orderbook" or "position"
    std::string symbol;
    std::weak_ptr<WebSocketSession> session;
};

namespace {
    std::vector<SubscriptionInfo> subscriptions;

    void cleanupDeadSubscriptions() {
        subscriptions.erase(
            std::remove_if(
                subscriptions.begin(),
                subscriptions.end(),
                [](const SubscriptionInfo& sub) { return sub.session.expired(); }
            ),
            subscriptions.end()
        );
    }

    void broadcastToSubscribers(const json& data, const std::string& type, const std::string& symbol, 
                              const std::shared_ptr<WebSocketServer>& server) {
        cleanupDeadSubscriptions();
        
        for (const auto& subscription : subscriptions) {
            if (auto session = subscription.session.lock()) {
                if (subscription.type == type && subscription.symbol == symbol) {
                    session->send(data.dump());
                }
            }
        }
    }
}

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
    server->onMessage([this](std::shared_ptr<WebSocketSession> session, const std::string& message) {
        try {
            json j = json::parse(message);
            std::cout << "Client request received: " << j.dump(2) << std::endl;
            
            if (j.contains("method")) {
                const std::string& method = j["method"];
                if (j.contains("symbol")) {
                    const std::string& symbol = j["symbol"];
                    
                    if (method == "subscribe_orderbook") {
                        handleOrderBookSubscription(symbol);
                        // Add to subscriptions
                        subscriptions.push_back({"orderbook", symbol, session});
                    }
                    else if (method == "subscribe_position") {
                        // Handle position subscription
                        if (!client || !isConnected()) {
                            std::cerr << "Cannot subscribe - not connected to Deribit" << std::endl;
                            return;
                        }

                        try {
                            json subscribeMsg = {
                                {"method", "private/subscribe"},
                                {"params", {
                                    {"channels", {
                                        "user.position." + symbol
                                    }}
                                }},
                                {"jsonrpc", "2.0"},
                                {"id", 124}
                            };
                            
                            std::cout << "Subscribing to position updates for " << symbol << std::endl;
                            if (client) {
                                client->sendMessage(subscribeMsg.dump());
                            }
                            // Add to subscriptions
                            subscriptions.push_back({"position", symbol, session});
                        } catch (const std::exception& e) {
                            std::cerr << "Error subscribing to position updates: " << e.what() << std::endl;
                        }
                    }
                }
            }
        } catch (const json::parse_error& e) {
            std::cout << "Invalid message from client: " << message << std::endl;
        }
    });

    server->onDisconnect([](std::shared_ptr<WebSocketSession> session) {
        cleanupDeadSubscriptions();
    });
}

void WebSocketManager::handleOrderBookSubscription(const std::string& symbol) {
    if (!client || !isConnected()) {
        std::cerr << "Cannot subscribe - not connected to Deribit" << std::endl;
        return;
    }

    try {
                    json subscribeMsg = {
            {"method", "public/subscribe"},
            {"params", {
                {"channels", {"book." + symbol + ".100ms"}}
            }},
            {"jsonrpc", "2.0"},
            {"id", 123}
        };
        
        std::cout << "Subscribing to orderbook for " << symbol << std::endl;
        if (client) {
            client->sendMessage(subscribeMsg.dump());
        }
    } catch (const std::exception& e) {
        std::cerr << "Error subscribing to orderbook: " << e.what() << std::endl;
    }
}

void WebSocketManager::setupDeribitClient() {
    client->onOpen([this]() {
        std::cout << "\nDeribit WebSocket connected!" << std::endl;
        connected = true;
    });

    client->onMessage([this](const std::string& message) {
        try {
            json j = json::parse(message);
            // std::cout << "Received from Deribit: " << j.dump(2) << std::endl;
            
            // Handle subscription confirmation
            if (j.contains("id")) {
                std::cout << "Subscription response: " << j.dump(2) << std::endl;
                if (j.contains("error")) {
                    std::cerr << "Subscription error: " << j["error"].dump(2) << std::endl;
                }
                return;
            }
            
            // Handle orderbook data
            if (j.contains("params")) {
                if (j["params"].contains("channel") && j["params"].contains("data")) {
                    std::string channel = j["params"]["channel"];
                    
                    // Parse channel to determine type and symbol
                    if (channel.substr(0, 5) == "book.") {
                        size_t pos = channel.find('.');
                        std::string symbol = channel.substr(pos + 1);
                        pos = symbol.find('.');
                        symbol = symbol.substr(0, pos);
                        
                        json orderbook = j["params"]["data"];
                        broadcastToSubscribers(orderbook, "orderbook", symbol, server);
                    }
                    else if (channel.substr(0, 13) == "user.position.") {
                        std::string symbol = channel.substr(13);
                        json position = j["params"]["data"];
                        broadcastToSubscribers(position, "position", symbol, server);
                    }
                }
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
    std::cout << "Starting local WebSocket server..." << std::endl;
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