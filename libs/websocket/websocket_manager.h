#ifndef WEBSOCKET_MANAGER_H
#define WEBSOCKET_MANAGER_H

#include "websocket_client.h"
#include "websocket_server.h"
#include "order_placement.h"
#include <memory>
#include <atomic>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * @brief Class to manage WebSocket connections and operations
 */
class WebSocketManager {
public:
    /**
     * @brief Construct a new WebSocketManager object
     * 
     * @param server_address The address of the server
     * @param server_port The port of the server
     */
    WebSocketManager(const std::string& server_address, unsigned short server_port);

    /**
     * @brief Destroy the WebSocketManager object
     */
    ~WebSocketManager();

    /**
     * @brief Start the WebSocket manager
     */
    void start();

    /**
     * @brief Stop the WebSocket manager
     */
    void stop();

    /**
     * @brief Check if the WebSocket manager is running
     * 
     * @return true if running, false otherwise
     */
    bool isRunning() const { return running; }

    /**
     * @brief Check if the WebSocket client is connected
     * 
     * @return true if connected, false otherwise
     */
    bool isConnected() const { return connected; }

    /**
     * @brief Connect to Deribit WebSocket
     * 
     * @param host The host address
     * @param port The port number
     * @param path The path for the WebSocket connection
     */
    void connectToDeribit(const std::string& host, const std::string& port, const std::string& path);

    /**
     * @brief Send a message to Deribit WebSocket
     * 
     * @param message The message to send
     */
    void sendToDeribit(const std::string& message);

    /**
     * @brief Handle order book subscription
     * 
     * @param symbol The symbol to subscribe to
     */
    void handleOrderBookSubscription(const std::string& symbol);

private:
    std::atomic<bool> running{true}; /**< Flag to indicate if the manager is running */
    std::atomic<bool> connected{false}; /**< Flag to indicate if the client is connected */
    std::shared_ptr<WebSocketServer> server; /**< WebSocket server instance */
    std::unique_ptr<WebSocketClient> client; /**< WebSocket client instance */
    OrderPlacement orderHandler; /**< Order placement handler */

    /**
     * @brief Setup the Deribit WebSocket client
     */
    void setupDeribitClient();

    /**
     * @brief Setup the local WebSocket server
     */
    void setupLocalServer();
};

#endif // WEBSOCKET_MANAGER_H