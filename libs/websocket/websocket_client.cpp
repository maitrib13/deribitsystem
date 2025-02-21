#include "websocket_client.h"
#include "root_certificates.hpp"
#include <iostream>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/ssl.hpp>

WebSocketClient::WebSocketClient()
    : resolver(ioContext)
    , sslContext(ssl::context::tlsv12_client)
    , isConnected(false) {
    
    // Load root certificates
    load_root_certificates(sslContext);
    
    // Enable certificate verification
    sslContext.set_verify_mode(ssl::verify_peer);
}

WebSocketClient::~WebSocketClient() {
    close();
    if (ioThread && ioThread->joinable()) {
        ioThread->join();
    }
}

void WebSocketClient::connect(const std::string& host, const std::string& port, const std::string& path) {
    try {
        // Create new WebSocket stream
        ws = std::make_unique<websocket::stream<ssl::stream<asio::ip::tcp::socket>>>(ioContext, sslContext);

        // Look up the domain name
        auto const results = resolver.resolve(host, port);

        // Make the connection on the IP address we get from a lookup
        auto ep = asio::connect(beast::get_lowest_layer(*ws), results);

        // Set SNI Hostname (many hosts need this to handshake successfully)
        if(!SSL_set_tlsext_host_name(ws->next_layer().native_handle(), host.c_str())) {
            throw beast::system_error(
                static_cast<int>(::ERR_get_error()),
                asio::error::get_ssl_category());
        }

        // Perform the SSL handshake
        ws->next_layer().handshake(ssl::stream_base::client);

        // Set a decorator to change the User-Agent of the handshake
        ws->set_option(websocket::stream_base::decorator(
            [](websocket::request_type& req) {
                req.set(beast::http::field::user_agent,
                    std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client");
            }));

        // Perform the websocket handshake
        ws->handshake(host + ":" + std::to_string(ep.port()), path);
        
        isConnected = true;
        // std::cout << "Connected to: " << host << std::endl;

        // Start the read loop in a separate thread
        ioThread = std::make_unique<std::thread>(&WebSocketClient::readLoop, this);

        // Call the onOpen callback if set
        if (openHandler) {
            openHandler();
        }
    }
    catch (const std::exception& e) {
        handleError(std::string("Connection error: ") + e.what());
    }
}

void WebSocketClient::sendMessage(const std::string& message) {
    if (!isConnected || !ws) {
        handleError("Not connected");
        return;
    }

    try {
        ws->write(asio::buffer(message));
    }
    catch (const std::exception& e) {
        handleError(std::string("Send error: ") + e.what());
    }
}

void WebSocketClient::close() {
    if (!ws) return;
    
    try {
        // Stop the read loop first
        stop();
        
        // Set timeout for the close operation
        beast::websocket::stream_base::timeout opt{
            std::chrono::seconds(1),  // handshake timeout
            std::chrono::seconds(1),  // idle timeout
            false                     // keep alive disabled during shutdown
        };
        ws->set_option(opt);
        
        // Send close frame with normal close code
        beast::error_code ec;
        ws->close(websocket::close_code::normal, ec);
        
        if (ec && ec != beast::errc::operation_canceled) {
            throw boost::system::system_error(ec);
        }
        
    } catch (const std::exception& e) {
        // Only log if it's not a typical shutdown error
        std::string error = e.what();
        if (error.find("Operation canceled") == std::string::npos &&
            error.find("stream truncated") == std::string::npos &&
            error.find("End of file") == std::string::npos) {
            if (errorHandler) {
                errorHandler(std::string("Close error: ") + error);
            }
        }
    }
}

void WebSocketClient::onOpen(std::function<void()> callback) {
    openHandler = std::move(callback);
}

void WebSocketClient::onMessage(std::function<void(const std::string&)> callback) {
    messageHandler = std::move(callback);
}

void WebSocketClient::onClose(std::function<void()> callback) {
    closeHandler = std::move(callback);
}

void WebSocketClient::onError(std::function<void(const std::string&)> callback) {
    errorHandler = std::move(callback);
}

void WebSocketClient::readLoop() {
    while (!shouldStop) {
        try {
            beast::flat_buffer buffer;
            ws->read(buffer);
            
            if (messageHandler) {
                messageHandler(beast::buffers_to_string(buffer.data()));
            }
        }
        catch (const boost::system::system_error& e) {
            if (shouldStop) {
                // Expected error during shutdown, ignore
                break;
            }
            if (errorHandler) {
                errorHandler(std::string("Read error: ") + e.what());
            }
            break;
        }
        catch (const std::exception& e) {
            if (errorHandler) {
                errorHandler(std::string("Read error: ") + e.what());
            }
            break;
        }
    }
}

void WebSocketClient::handleError(const std::string& error) {
    std::cerr << error << std::endl;
    if (errorHandler) {
        errorHandler(error);
    }
}