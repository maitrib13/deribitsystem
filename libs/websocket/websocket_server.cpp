#include "websocket_server.h"
#include <iostream>

WebSocketServer::WebSocketServer(const std::string& address, unsigned short port)
    : acceptor(ioc)
    , running(false) {
    
    auto endpoint = tcp::endpoint(net::ip::make_address(address), port);
    acceptor.open(endpoint.protocol());
    acceptor.set_option(net::socket_base::reuse_address(true));
    acceptor.bind(endpoint);
    acceptor.listen(net::socket_base::max_listen_connections);
}

WebSocketServer::~WebSocketServer() {
    stop();
}

void WebSocketServer::run() {
    running = true;
    doAccept();

    threads.reserve(std::thread::hardware_concurrency());
    for(std::size_t i = 0; i < threads.capacity(); ++i) {
        threads.emplace_back([this] { ioc.run(); });
    }
}

void WebSocketServer::stop() {
    if (!running) return;
    
    running = false;
    ioc.stop();

    for(auto& thread : threads) {
        if(thread.joinable()) {
            thread.join();
        }
    }
    threads.clear();

    std::lock_guard<std::mutex> lock(sessionsMutex);
    for(auto& session : sessions) {
        beast::error_code ec;
        session->ws().close(websocket::close_code::normal, ec);
    }
    sessions.clear();
}

void WebSocketServer::broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    for(auto& session : sessions) {
        session->send(message);
    }
}

void WebSocketServer::doAccept() {
    auto session = std::make_shared<WebSocketSession>(*this, ioc);
    
    acceptor.async_accept(
        session->ws().next_layer(),
        [this, session](beast::error_code ec) {
            if (!ec) {
                {
                    std::lock_guard<std::mutex> lock(sessionsMutex);
                    sessions.insert(session);
                }
                session->start();
                if (connectHandler) {
                    connectHandler(session);
                }
            } else if (errorHandler) {
                errorHandler("Accept error: " + ec.message());
            }

            if (running) {
                doAccept();
            }
        });
}

void WebSocketServer::removeSession(std::shared_ptr<WebSocketSession> session) {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    sessions.erase(session);
}

void WebSocketServer::onConnect(std::function<void(std::shared_ptr<WebSocketSession>)> callback) {
    connectHandler = std::move(callback);
}

void WebSocketServer::onMessage(std::function<void(std::shared_ptr<WebSocketSession>, const std::string&)> callback) {
    messageHandler = std::move(callback);
}

void WebSocketServer::onDisconnect(std::function<void(std::shared_ptr<WebSocketSession>)> callback) {
    disconnectHandler = std::move(callback);
}

void WebSocketServer::onError(std::function<void(const std::string&)> callback) {
    errorHandler = std::move(callback);
}

// WebSocketSession implementation
WebSocketSession::WebSocketSession(WebSocketServer& server, net::io_context& ioc)
    : server(server)
    , ws_(ioc) {
}

void WebSocketSession::start() {
    ws_.set_option(websocket::stream_base::timeout::suggested(
        beast::role_type::server));

    ws_.set_option(websocket::stream_base::decorator(
        [](websocket::response_type& res) {
            res.set(http::field::server,
                   std::string(BOOST_BEAST_VERSION_STRING) +
                   " websocket-server");
        }));

    ws_.async_accept(
        beast::bind_front_handler(
            [this](beast::error_code ec) {
                if(ec) {
                    if(server.errorHandler) {
                        server.errorHandler("WebSocket Accept error: " + ec.message());
                    }
                    return;
                }
                doRead();
            }));
}

void WebSocketSession::doRead() {
    ws_.async_read(
        buffer,
        beast::bind_front_handler(
            &WebSocketSession::onRead,
            shared_from_this()));
}

void WebSocketSession::onRead(beast::error_code ec, std::size_t bytes_transferred) {
    if(ec == websocket::error::closed) {
        if(server.disconnectHandler) {
            server.disconnectHandler(shared_from_this());
        }
        server.removeSession(shared_from_this());
        return;
    }

    if(ec) {
        if(server.errorHandler) {
            server.errorHandler("Read error: " + ec.message());
        }
        return;
    }

    if(server.messageHandler) {
        server.messageHandler(
            shared_from_this(),
            beast::buffers_to_string(buffer.data()));
    }

    buffer.consume(buffer.size());
    doRead();
}

void WebSocketSession::send(const std::string& message) {
    outgoing_message = message;
    ws_.async_write(
        net::buffer(outgoing_message),
        beast::bind_front_handler(
            &WebSocketSession::onWrite,
            shared_from_this()));
}

void WebSocketSession::onWrite(beast::error_code ec, std::size_t bytes_transferred) {
    if(ec) {
        if(server.errorHandler) {
            server.errorHandler("Write error: " + ec.message());
        }
        return;
    }

    outgoing_message.clear();
}