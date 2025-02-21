#ifndef ORDER_PLACEMENT_H
#define ORDER_PLACEMENT_H

#include <string>
#include <nlohmann/json.hpp>
#include "rest_client.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <future>

using json = nlohmann::json;

/**
 * @brief Structure to hold request information
 */
struct ApiRequest {
    std::string method; /**< HTTP method */
    json params; /**< Request parameters */
    std::promise<json> promise; /**< Promise to hold the response */
    std::chrono::steady_clock::time_point timestamp; /**< Timestamp of the request */
};

/**
 * @brief Enum to represent different response types
 */
enum class ResponseType {
    POSITION, /**< Position response */
    ACTIVE_ORDERS, /**< Active orders response */
    CANCELLED_ORDER, /**< Cancelled order response */
    ORDER_RESPONSE, /**< Order response */
    MODIFIED_ORDER, /**< Modified order response */
    INSTRUMENT, /**< Instrument response */
    ORDERBOOK /**< Orderbook response */
};

/**
 * @brief Class to handle order placement and related operations
 */
class OrderPlacement {
public:
    /**
     * @brief Construct a new OrderPlacement object
     */
    OrderPlacement();

    /**
     * @brief Destroy the OrderPlacement object
     */
    ~OrderPlacement();
    
    /**
     * @brief Place a new order
     * 
     * @param instrument The trading instrument
     * @param side The side of the order ("buy" or "sell")
     * @param type The type of the order ("market", "limit", "stop_market", "stop_limit")
     * @param amount The amount to trade
     * @param price The price for limit orders (default is 0.0)
     * @param reduceOnly Whether the order is reduce-only (default is false)
     * @return std::future<json> The response from the server
     */
    std::future<json> placeOrder(const std::string& instrument, 
                                const std::string& side,        
                                const std::string& type,        
                                double amount,
                                double price = 0.0,
                                bool reduceOnly = false);
                   
    /**
     * @brief Cancel an existing order
     * 
     * @param orderId The ID of the order to cancel
     * @return std::future<json> The response from the server
     */
    std::future<json> cancelOrder(const std::string& orderId);
    
    /**
     * @brief Modify an existing order
     * 
     * @param orderId The ID of the order to modify
     * @param newPrice The new price for the order
     * @param newAmount The new amount for the order
     * @return std::future<json> The response from the server
     */
    std::future<json> modifyOrder(const std::string& orderId,
                                 double newPrice,
                                 double newAmount);
                    
    /**
     * @brief Get active orders
     * 
     * @return std::future<json> The response from the server
     */
    std::future<json> getActiveOrders();

    /**
     * @brief Get the state of an order
     * 
     * @param orderId The ID of the order
     * @return std::future<json> The response from the server
     */
    std::future<json> getOrderState(const std::string& orderId);

    /**
     * @brief Get the order book for an instrument
     * 
     * @param instrument The trading instrument
     * @return std::future<json> The response from the server
     */
    std::future<json> getOrderbook(const std::string& instrument);

    /**
     * @brief Get the available instruments for a currency and kind
     * 
     * @param currency The currency (e.g., "BTC")
     * @param kind The kind of instrument (e.g., "future", "option")
     * @return std::future<json> The response from the server
     */
    std::future<json> getInstruments(const std::string& currency, const std::string& kind);

    /**
     * @brief Get the positions for a currency
     * 
     * @param currency The currency (e.g., "BTC")
     * @return std::future<json> The response from the server
     */
    std::future<json> getPositions(const std::string& currency);

    /**
     * @brief Get the details of an instrument
     * 
     * @param instrument_name The name of the instrument
     * @return std::future<json> The response from the server
     */
    std::future<json> getInstrumentDetails(const std::string& instrument_name);

    /**
     * @brief Print the response in a formatted manner
     * 
     * @param response The response from the server
     * @param type The type of the response
     * @param extraInfo Additional information to print
     */
    void printResponse(const json& response, ResponseType type, const std::string& extraInfo = "");

private:
    RestClient client; /**< REST client for making requests */
    std::string apiKey; /**< API key for authentication */
    std::string apiSecret; /**< API secret for authentication */
    std::string access_token; /**< Access token for authentication */
    std::string refresh_token; /**< Refresh token for authentication */
    std::string baseUrl; /**< Base URL for the API */
    int token_expiry; /**< Token expiry time */
    std::chrono::steady_clock::time_point last_auth_time; /**< Last authentication time */
    
    // Thread management
    std::thread workerThread; /**< Worker thread for processing requests */
    std::queue<std::unique_ptr<ApiRequest>> requestQueue; /**< Queue to hold API requests */
    std::mutex queueMutex; /**< Mutex for synchronizing access to the queue */
    std::condition_variable queueCV; /**< Condition variable for queue synchronization */
    bool running; /**< Flag to indicate if the worker thread is running */
    
    /**
     * @brief Setup authentication for the client
     */
    void setupAuth();
    
    /**
     * @brief Sign a message for authentication
     * 
     * @param message The message to sign
     * @return std::string The signed message
     */
    std::string sign(const std::string& message);
    
    /**
     * @brief Send an authenticated request to the server
     * 
     * @param method The HTTP method
     * @param params The parameters for the request
     * @return json The response from the server
     */
    json sendAuthenticatedRequest(const std::string& method, const json& params);
    
    /**
     * @brief Thread worker function to process requests
     */
    void processRequests();
    
    /**
     * @brief Authenticate the client
     */
    void authenticate();
    
    /**
     * @brief Helper to queue requests
     * 
     * @param method The HTTP method
     * @param params The parameters for the request
     * @return std::future<json> The response from the server
     */
    std::future<json> queueRequest(const std::string& method, const json& params);
    
    /**
     * @brief Start the worker thread
     */
    void startWorker();
    
    /**
     * @brief Stop the worker thread
     */
    void stopWorker();
    
    /**
     * @brief Print the details of an instrument
     * 
     * @param instrument The instrument details
     */
    void printInstrumentDetails(const json& instrument);
    
    /**
     * @brief Print the details of an order
     * 
     * @param order The order details
     * @param isCancelled Whether the order is cancelled
     */
    void printOrderDetails(const json& order, bool isCancelled = false);
    
    /**
     * @brief Print the details of a trade
     * 
     * @param trade The trade details
     */
    void printTradeDetails(const json& trade);
    
    /**
     * @brief Print the details of a position
     * 
     * @param position The position details
     */
    void printPositionDetails(const json& position);
    
    /**
     * @brief Print the details of an orderbook
     * 
     * @param orderbook The orderbook details
     */
    void printOrderbookDetails(const json& orderbook);
};

#endif // ORDER_PLACEMENT_H