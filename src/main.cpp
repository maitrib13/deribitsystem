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
std::shared_ptr<WebSocketServer> server; // Global server pointer
WebSocketClient client;

/**
 * @brief Signal handler for graceful shutdown
 * 
 * @param signum Signal number
 */
void signalHandler(int signum)
{
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    running = false;
}

// Function to safely shutdown the WebSocket client
void safeShutdown(WebSocketClient &client, std::atomic<bool> &connected)
{
    if (connected)
    {
        try
        {
            std::cout << "Initiating graceful shutdown..." << std::endl;

            // First, stop the read loop
            client.stop();

            // Wait a moment for the read loop to stop
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            // Send close frame if still connected
            if (connected)
            {
                client.close();
            }

            // Wait for close to complete
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            connected = false;
            running = false;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Shutdown warning: " << e.what() << std::endl;
        }
    }
}

// Function to print available commands
void printHelp()
{
    std::cout << "\nAvailable Commands:\n"
              << "----------------------------------------\n"
              << "Instrument Commands:\n"
              << "  instrument <symbol>     - Get instrument details\n"
              << "\nTrading Commands:\n"
              << "  buy <instrument> <type> <amount> [price]  - Place buy order\n"
              << "  sell <instrument> <type> <amount> [price] - Place sell order\n"
              << "  cancel <order_id>       - Cancel specific order\n"
              << "  modify <order_id> <new_price> <new_amount> - Modify existing order\n"
              << "\nInformation Commands:\n"
              << "  orders                  - Get active orders (optional: for specific instrument)\n"
              << "  orderbook <instrument>  - Get orderbook\n"
              << "  positions <currency>    - Get positions\n"
              << "\nOther Commands:\n"
              << "  help                    - Show this help\n"
              << "  quit                    - Exit program\n"
              << "----------------------------------------\n";
}

int main()
{
    try
    {
        // Load environment variables from .env file
        if (!EnvHandler::loadEnvFile())
        {
            std::cerr << "Failed to load .env file" << std::endl;
            return 1;
        }

        // Create order placement handler
        OrderPlacement orderHandler;

        // Create and start WebSocket manager
        WebSocketManager wsManager("0.0.0.0", 8000);
        wsManager.start();

        std::cout << "\nConnecting to Deribit..." << std::endl;
        wsManager.connectToDeribit("www.deribit.com", "443", "/ws/api/v2");

        // Print available commands
        printHelp();

        std::string input;
        while (wsManager.isRunning() && std::getline(std::cin, input))
        {
            if (input == "quit")
            {
                break;
            }
            else if (input == "help")
            {
                printHelp();
            }
            else if (input.substr(0, 10) == "instrument")
            {
                try
                {
                    std::istringstream iss(input);
                    std::string cmd, currency, kind;
                    iss >> cmd >> currency >> kind;

                    if (currency.empty() || kind.empty())
                    {
                        std::cout << "Usage: instrument <currency> <kind>" << std::endl;
                        std::cout << "Example: instruments BTC_USDT future" << std::endl;
                        std::cout << "Available kinds: future, option, spot" << std::endl;
        
                        continue;
                    }

                    std::cout << "Getting instrument for " << currency << " " << kind << "..." << std::endl;
                    auto future = orderHandler.getInstruments(currency, kind);
                    std::future_status status = future.wait_for(std::chrono::seconds(30));

                    if (status == std::future_status::timeout)
                    {
                        throw std::runtime_error("Request timed out after 30 seconds");
                    }
                    json response = future.get();
                    orderHandler.printResponse(response, ResponseType::INSTRUMENT);
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Error getting instruments: " << e.what() << std::endl;
                }
            }
            else if (input.substr(0, 10) == "orderbook ")
            {
                try
                {
                    std::istringstream iss(input);
                    std::string cmd, instrument;
                    iss >> cmd >> instrument;

                    if (instrument.empty())
                    {
                        std::cout << "Usage: orderbook <instrument>" << std::endl;
                        continue;
                    }

                    auto future = orderHandler.getOrderbook(instrument);
                    std::future_status status = future.wait_for(std::chrono::seconds(30));

                    if (status == std::future_status::timeout)
                    {
                        throw std::runtime_error("Request timed out after 30 seconds");
                    }
                    json response = future.get();
                    orderHandler.printResponse(response, ResponseType::ORDERBOOK);
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Error getting orderbook: " << e.what() << std::endl;
                }
            }
            else if (input.substr(0, 10) == "positions ")
            {
                try
                {
                    std::istringstream iss(input);
                    std::string cmd, currency;
                    iss >> cmd >> currency;

                    if (currency.empty())
                    {
                        std::cout << "Usage: positions <currency>" << std::endl;
                        std::cout << "Example: positions BTC" << std::endl;
                        continue;
                    }

                    std::cout << "Getting positions for " << currency << "..." << std::endl;
                    auto future = orderHandler.getPositions(currency);
                    std::future_status status = future.wait_for(std::chrono::seconds(30));

                    if (status == std::future_status::timeout)
                    {
                        throw std::runtime_error("Request timed out after 30 seconds");
                    }

                    json response = future.get();
                    orderHandler.printResponse(response, ResponseType::POSITION);
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Error getting positions: " << e.what() << std::endl;
                }
            }
            else if (input.substr(0, 11) == "orderstatus ")
            {
                try
                {
                    std::istringstream iss(input);
                    std::string cmd, orderId;
                    iss >> cmd >> orderId;

                    if (orderId.empty())
                    {
                        std::cout << "Usage: orderstatus <order_id>" << std::endl;
                        continue;
                    }

                    auto future = orderHandler.getOrderState(orderId);
                    std::future_status status = future.wait_for(std::chrono::seconds(30));

                    if (status == std::future_status::timeout)
                    {
                        throw std::runtime_error("Request timed out after 30 seconds");
                    }
                    json response = future.get();
                    orderHandler.printResponse(response, ResponseType::INSTRUMENT);
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Error getting order status: " << e.what() << std::endl;
                }
            }
            else if (input.substr(0, 7) == "modify ")
            {
                try
                {
                    std::istringstream iss(input);
                    std::string cmd, orderId, newPriceStr, newAmountStr;
                    iss >> cmd >> orderId >> newPriceStr >> newAmountStr;

                    if (orderId.empty() || newPriceStr.empty() || newAmountStr.empty())
                    {
                        std::cout << "Usage: modify <order_id> <new_price> <new_amount>" << std::endl;
                        continue;
                    }

                    double newPrice = std::stod(newPriceStr);
                    double newAmount = std::stod(newAmountStr);

                    auto future = orderHandler.modifyOrder(orderId, newPrice, newAmount);
                    std::future_status status = future.wait_for(std::chrono::seconds(30));

                    if (status == std::future_status::timeout)
                    {
                        throw std::runtime_error("Request timed out after 30 seconds");
                    }
                    json response = future.get();
                    orderHandler.printResponse(response, ResponseType::MODIFIED_ORDER);
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Error modifying order: " << e.what() << std::endl;
                }
            }
            else if (input == "orders")
            {
                try
                {
                    std::cout << "Getting active orders"
                              << "..." << std::endl;

                    auto future = orderHandler.getActiveOrders();
                    std::future_status status = future.wait_for(std::chrono::seconds(30));

                    if (status == std::future_status::timeout)
                    {
                        throw std::runtime_error("Request timed out after 30 seconds");
                    }

                    json response = future.get();
                    orderHandler.printResponse(response, ResponseType::ACTIVE_ORDERS);
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Error getting orders: " << e.what() << std::endl;
                }
            }
            else if (input.substr(0, 6) == "cancel" && input.length() > 7)
            {
                try
                {
                    std::string orderId = input.substr(7);
                    auto future = orderHandler.cancelOrder(orderId);
                    std::future_status status = future.wait_for(std::chrono::seconds(30));

                    if (status == std::future_status::timeout)
                    {
                        throw std::runtime_error("Request timed out after 30 seconds");
                    }
                    json response = future.get();
                    orderHandler.printResponse(response, ResponseType::CANCELLED_ORDER);
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Error cancelling order: " << e.what() << std::endl;
                }
            }
            else if (input.substr(0, 3) == "buy" || input.substr(0, 4) == "sell")
            {
                try
                {
                    std::istringstream iss(input);
                    std::string side, instrument, type, amountStr, priceStr;
                    iss >> side >> instrument >> type >> amountStr;
                    double amount = std::stod(amountStr);
                    double price = 0.0;

                    if (type == "limit")
                    {
                        iss >> priceStr;
                        price = std::stod(priceStr);
                    }

                    auto future = orderHandler.placeOrder(
                        instrument, // Use the provided instrument
                        side,
                        type,
                        amount,
                        price);
                    std::future_status status = future.wait_for(std::chrono::seconds(30));

                    if (status == std::future_status::timeout)
                    {
                        throw std::runtime_error("Request timed out after 30 seconds");
                    }
                    json response = future.get();
                    orderHandler.printResponse(response, ResponseType::ORDER_RESPONSE);
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Error placing order: " << e.what() << std::endl;
                }
            }
            else if (!input.empty() && wsManager.isConnected())
            {
                wsManager.sendToDeribit(input);
            }
        }

        // Cleanup
        wsManager.stop();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Main exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}