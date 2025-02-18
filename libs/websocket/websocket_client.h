#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/ssl.hpp>
#include <functional>
#include <string>
#include <thread>
#include <memory>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace ssl = boost::asio::ssl;

class WebSocketClient {
public:
    /**
     * @brief Construct a new WebSocketClient object
     * 
     */
    WebSocketClient();

    /**
     * @brief Destroy the WebSocketClient object
     * 
     */
    ~WebSocketClient();

    /**
     * @brief Connect to the WebSocket server
     * 
     * @param host The host address of the WebSocket server
     * @param port The port number of the WebSocket server
     * @param path The path for the WebSocket connection
     */
    void connect(const std::string& host, const std::string& port = "443", const std::string& path = "/");

    /**
     * @brief Send a message to the WebSocket server
     * 
     * @param message The message to be sent
     */
    void sendMessage(const std::string& message);

    /**
     * @brief Close the WebSocket connection
     * 
     */
    void close();

    /**
     * @brief Set the callback function to be called when the connection is opened
     * 
     * @param callback The callback function
     */
    void onOpen(std::function<void()> callback);

    /**
     * @brief Set the callback function to be called when a message is received
     * 
     * @param callback The callback function
     */
    void onMessage(std::function<void(const std::string&)> callback);

    /**
     * @brief Set the callback function to be called when the connection is closed
     * 
     * @param callback The callback function
     */
    void onClose(std::function<void()> callback);

    /**
     * @brief Set the callback function to be called when an error occurs
     * 
     * @param callback The callback function
     */
    void onError(std::function<void(const std::string&)> callback);

    void stop() {
        shouldStop = true;
    }

private:
    asio::io_context ioContext;
    std::unique_ptr<std::thread> ioThread;
    asio::ip::tcp::resolver resolver;
    std::unique_ptr<websocket::stream<ssl::stream<asio::ip::tcp::socket>>> ws;
    ssl::context sslContext;

    std::function<void()> openHandler;
    std::function<void(const std::string&)> messageHandler;
    std::function<void()> closeHandler;
    std::function<void(const std::string&)> errorHandler;
    std::atomic<bool> shouldStop{false};

    void readLoop();
    bool isConnected;
    void handleError(const std::string& error);
};

#endif // WEBSOCKET_CLIENT_H
