#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <functional>
#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <unordered_set>
#include <mutex>
#include "env_handler.h"

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace http = beast::http;
using tcp = boost::asio::ip::tcp;

// Forward declaration
class WebSocketSession;

/**
 * @brief Class to manage WebSocket server operations
 */
class WebSocketServer {
public:
    /**
     * @brief Construct a new WebSocketServer object
     * 
     * @param address The address to bind the server
     * @param port The port to bind the server
     */
    WebSocketServer(const std::string& address, unsigned short port);

    /**
     * @brief Destroy the WebSocketServer object
     */
    ~WebSocketServer();

    /**
     * @brief Run the WebSocket server
     */
    void run();

    /**
     * @brief Stop the WebSocket server
     */
    void stop();

    /**
     * @brief Broadcast a message to all connected clients
     * 
     * @param message The message to broadcast
     */
    void broadcast(const std::string& message);

    /**
     * @brief Set the callback for new connections
     * 
     * @param callback The callback function
     */
    void onConnect(std::function<void(std::shared_ptr<WebSocketSession>)> callback);

    /**
     * @brief Set the callback for incoming messages
     * 
     * @param callback The callback function
     */
    void onMessage(std::function<void(std::shared_ptr<WebSocketSession>, const std::string&)> callback);

    /**
     * @brief Set the callback for disconnections
     * 
     * @param callback The callback function
     */
    void onDisconnect(std::function<void(std::shared_ptr<WebSocketSession>)> callback);

    /**
     * @brief Set the callback for errors
     * 
     * @param callback The callback function
     */
    void onError(std::function<void(const std::string&)> callback);

private:
    /**
     * @brief Accept new connections
     */
    void doAccept();

    /**
     * @brief Remove a session from the active sessions
     * 
     * @param session The session to remove
     */
    void removeSession(std::shared_ptr<WebSocketSession> session);

    net::io_context ioc; /**< IO context for asynchronous operations */
    tcp::acceptor acceptor; /**< TCP acceptor for incoming connections */
    std::vector<std::thread> threads; /**< Threads for handling connections */
    bool running; /**< Flag to indicate if the server is running */

    std::unordered_set<std::shared_ptr<WebSocketSession>> sessions; /**< Set of active sessions */
    std::mutex sessionsMutex; /**< Mutex for synchronizing access to sessions */

    std::function<void(std::shared_ptr<WebSocketSession>)> connectHandler; /**< Callback for new connections */
    std::function<void(std::shared_ptr<WebSocketSession>, const std::string&)> messageHandler; /**< Callback for incoming messages */
    std::function<void(std::shared_ptr<WebSocketSession>)> disconnectHandler; /**< Callback for disconnections */
    std::function<void(const std::string&)> errorHandler; /**< Callback for errors */

    friend class WebSocketSession;
};

/**
 * @brief Class to manage individual WebSocket sessions
 */
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    /**
     * @brief Construct a new WebSocketSession object
     * 
     * @param server Reference to the WebSocket server
     * @param ioc IO context for asynchronous operations
     */
    WebSocketSession(WebSocketServer& server, net::io_context& ioc);
    
    /**
     * @brief Get the WebSocket stream
     * 
     * @return websocket::stream<tcp::socket>& Reference to the WebSocket stream
     */
    websocket::stream<tcp::socket>& ws() { return ws_; }

    /**
     * @brief Start the WebSocket session
     */
    void start();

    /**
     * @brief Send a message to the client
     * 
     * @param message The message to send
     */
    void send(const std::string& message);

private:
    /**
     * @brief Read data from the client
     */
    void doRead();

    /**
     * @brief Handle read completion
     * 
     * @param ec Error code
     * @param bytes_transferred Number of bytes transferred
     */
    void onRead(beast::error_code ec, std::size_t bytes_transferred);

    /**
     * @brief Write data to the client
     * 
     * @param message The message to write
     */
    void doWrite(const std::string& message);

    /**
     * @brief Handle write completion
     * 
     * @param ec Error code
     * @param bytes_transferred Number of bytes transferred
     */
    void onWrite(beast::error_code ec, std::size_t bytes_transferred);

    WebSocketServer& server; /**< Reference to the WebSocket server */
    websocket::stream<tcp::socket> ws_; /**< WebSocket stream */
    beast::flat_buffer buffer; /**< Buffer for reading data */
    std::string outgoing_message; /**< Message to send */
    bool use_binary_; /**< Flag to indicate if binary mode is used */
};

#endif // WEBSOCKET_SERVER_H
