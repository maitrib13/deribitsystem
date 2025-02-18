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

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace http = beast::http;
using tcp = boost::asio::ip::tcp;

// Forward declaration
class WebSocketSession;

class WebSocketServer {
public:
    WebSocketServer(const std::string& address, unsigned short port);
    ~WebSocketServer();

    void run();
    void stop();
    void broadcast(const std::string& message);

    void onConnect(std::function<void(std::shared_ptr<WebSocketSession>)> callback);
    void onMessage(std::function<void(std::shared_ptr<WebSocketSession>, const std::string&)> callback);
    void onDisconnect(std::function<void(std::shared_ptr<WebSocketSession>)> callback);
    void onError(std::function<void(const std::string&)> callback);

private:
    void doAccept();
    void removeSession(std::shared_ptr<WebSocketSession> session);

    net::io_context ioc;
    tcp::acceptor acceptor;
    std::vector<std::thread> threads;
    bool running;

    std::unordered_set<std::shared_ptr<WebSocketSession>> sessions;
    std::mutex sessionsMutex;

    std::function<void(std::shared_ptr<WebSocketSession>)> connectHandler;
    std::function<void(std::shared_ptr<WebSocketSession>, const std::string&)> messageHandler;
    std::function<void(std::shared_ptr<WebSocketSession>)> disconnectHandler;
    std::function<void(const std::string&)> errorHandler;

    friend class WebSocketSession;
};

class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    WebSocketSession(WebSocketServer& server, net::io_context& ioc);
    
    websocket::stream<tcp::socket>& ws() { return ws_; }
    void start();
    void send(const std::string& message);

private:
    void doRead();
    void onRead(beast::error_code ec, std::size_t bytes_transferred);
    void doWrite(const std::string& message);
    void onWrite(beast::error_code ec, std::size_t bytes_transferred);

    WebSocketServer& server;
    websocket::stream<tcp::socket> ws_;
    beast::flat_buffer buffer;
    std::string outgoing_message;
};

#endif // WEBSOCKET_SERVER_H
