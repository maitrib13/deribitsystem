#include "websocket_client.h"
#include "rest_client.h"
#include "order_placement.h"
#include "websocket_server.h"
#include "websocket_manager.h"
#include "env_handler.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
std::atomic<bool> running{true};
std::atomic<bool> connected{false};
std::shared_ptr<WebSocketServer> server;  // Global server pointer
WebSocketClient client;


void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    running = false;
}

// Function to get instrument details
void getInstrumentDetails(const std::string& instrument_name) {
    try {
        RestClient restClient;
        restClient.setHeader("Accept", "application/json");
        restClient.setTimeout(30);
        
        std::string url = "https://test.deribit.com/api/v2/public/get_instrument?instrument_name=" + instrument_name;
        std::string response = restClient.get(url);
        
        if (restClient.getLastResponseCode() == 200) {
            json responseJson = json::parse(response);
            std::cout << "\nInstrument Details:" << std::endl;
            std::cout << "Status: " << responseJson["result"]["status"] << std::endl;
            std::cout << "Settlement Period: " << responseJson["result"]["settlement_period"] << std::endl;
            std::cout << "Quote Currency: " << responseJson["result"]["quote_currency"] << std::endl;
            std::cout << "Min Trade Amount: " << responseJson["result"]["min_trade_amount"] << std::endl;
            std::cout << "Kind: " << responseJson["result"]["kind"] << std::endl;
            std::cout << "Is Active: " << (responseJson["result"]["is_active"] ? "Yes" : "No") << std::endl;
            std::cout << "Creation Timestamp: " << responseJson["result"]["creation_timestamp"] << std::endl;
            std::cout << "Base Currency: " << responseJson["result"]["base_currency"] << std::endl;
        } else {
            std::cout << "Error getting instrument details. Status code: " 
                      << restClient.getLastResponseCode() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error fetching instrument details: " << e.what() << std::endl;
    }
}

void safeShutdown(WebSocketClient& client, std::atomic<bool>& connected) {
    if (connected) {
        try {
            std::cout << "Initiating graceful shutdown..." << std::endl;
            
            // First, stop the read loop
            client.stop();
            
            // Wait a moment for the read loop to stop
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Send close frame if still connected
            if (connected) {
                client.close();
            }
            
            // Wait for close to complete
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            connected = false;
            running = false;
            
        } catch (const std::exception& e) {
            std::cerr << "Shutdown warning: " << e.what() << std::endl;
        }
    }
}

void printHelp() {
    std::cout << "\nAvailable Commands:" << std::endl;
    std::cout << "1. instrument - Fetch instrument details" << std::endl;
    std::cout << "2. buy - Place buy order (e.g., 'buy limit 0.1 220.0' , 'buy market 0.1')" << std::endl;
    std::cout << "3. sell - Place sell order (e.g., 'sell market 0.1','sell limit 0.1 220.0')" << std::endl;
    std::cout << "4. cancel <order_id> - Cancel specific order" << std::endl;
    std::cout << "5. orders - View active orders" << std::endl;
    std::cout << "6. help - Show this help message" << std::endl;
    std::cout << "7. quit - Exit program" << std::endl;
}

int main() {
    try {
        if (!EnvHandler::loadEnvFile()) {
            std::cerr << "Failed to load .env file" << std::endl;
            return 1;
        }

        // Create order placement handler
        OrderPlacement orderHandler;

        // Create and start WebSocket manager
        WebSocketManager wsManager("0.0.0.0", 8000);
        wsManager.start();

        std::cout << "Fetching BNB_USDC instrument details..." << std::endl;
        getInstrumentDetails("BNB_USDC");

        std::cout << "\nConnecting to Deribit..." << std::endl;
        wsManager.connectToDeribit("test.deribit.com", "443", "/ws/api/v2");

        printHelp();

        std::string input;
        while (wsManager.isRunning() && std::getline(std::cin, input)) {
            if (input == "quit") {
                break;
            } else if (input == "help") {
                printHelp();
            } else if (input == "instrument") {
                getInstrumentDetails("BNB_USDC");
            } else if (input == "orders") {
                try {
                    json response = orderHandler.getActiveOrders("BNB_USDC");
                    std::cout << "Active orders: " << response.dump(2) << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "Error getting orders: " << e.what() << std::endl;
                }
            } else if (input.substr(0, 6) == "cancel" && input.length() > 7) {
                try {
                    std::string orderId = input.substr(7);
                    json response = orderHandler.cancelOrder(orderId);
                    std::cout << "Cancel response: " << response.dump(2) << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "Error cancelling order: " << e.what() << std::endl;
                }
            } else if (input.substr(0, 3) == "buy" || input.substr(0, 4) == "sell") {
                try {
                    std::istringstream iss(input);
                    std::string side, type, amountStr, priceStr;
                    iss >> side >> type >> amountStr;
                    double amount = std::stod(amountStr);
                    double price = 0.0;
                    
                    if (type == "limit") {
                        iss >> priceStr;
                        price = std::stod(priceStr);
                    }

                    json response = orderHandler.placeOrder(
                        "BNB_USDC",
                        side,
                        type,
                        amount,
                        price
                    );
                    std::cout << "Order response: " << response.dump(2) << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "Error placing order: " << e.what() << std::endl;
                }
            } else if (!input.empty() && wsManager.isConnected()) {
                wsManager.sendToDeribit(input);
            }
        }

        // Cleanup
        wsManager.stop();

    } catch (const std::exception& e) {
        std::cerr << "Main exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}