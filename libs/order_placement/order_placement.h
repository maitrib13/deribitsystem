#ifndef ORDER_PLACEMENT_H
#define ORDER_PLACEMENT_H

#include <string>
#include <nlohmann/json.hpp>
#include "rest_client.h"

using json = nlohmann::json;

class OrderPlacement {
public:
    /**
     * @brief Construct a new OrderPlacement object
     * 
     */
    OrderPlacement();
    
    /**
     * @brief Place a new order
     * 
     * @param instrument The trading instrument
     * @param side The side of the order ("buy" or "sell")
     * @param type The type of the order ("market", "limit", "stop_market", "stop_limit")
     * @param amount The amount to trade
     * @param price The price for limit orders (default is 0.0)
     * @param reduceOnly Whether the order is reduce-only (default is false)
     * @return json The response from the server
     */
    json placeOrder(const std::string& instrument, 
                   const std::string& side,        
                   const std::string& type,        
                   double amount,
                   double price = 0.0,
                   bool reduceOnly = false);
                   
    /**
     * @brief Cancel an existing order
     * 
     * @param orderId The ID of the order to cancel
     * @return json The response from the server
     */
    json cancelOrder(const std::string& orderId);
    
    /**
     * @brief Modify an existing order
     * 
     * @param orderId The ID of the order to modify
     * @param newPrice The new price for the order
     * @param newAmount The new amount for the order
     * @return json The response from the server
     */
    json modifyOrder(const std::string& orderId,
                    double newPrice,
                    double newAmount);
                    
    /**
     * @brief Get active orders
     * 
     * @param instrument The trading instrument (optional)
     * @return json The response from the server
     */
    json getActiveOrders(const std::string& instrument = "");
    
    /**
     * @brief Get the state of an order
     * 
     * @param orderId The ID of the order
     * @return json The response from the server
     */
    json getOrderState(const std::string& orderId);
    
    /**
     * @brief Get the order book for an instrument
     * 
     * @param instrument The trading instrument
     * @return json The response from the server
     */
    json getOrderbook(const std::string& instrument);
    
    /**
     * @brief Get the available instruments for a currency and kind
     * 
     * @param currency The currency (e.g., "BTC")
     * @param kind The kind of instrument (e.g., "future", "option")
     * @return json The response from the server
     */
    json getInstruments(const std::string& currency, const std::string& kind);
    
    /**
     * @brief Get the positions for a currency
     * 
     * @param currency The currency (e.g., "BTC")
     * @return json The response from the server
     */
    json getPositions(const std::string& currency);

private:
    RestClient client;
    std::string apiKey;
    std::string apiSecret;
    
    /**
     * @brief Setup authentication for the client
     * 
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
    json sendAuthenticatedRequest(const std::string& method,
                                const json& params);
};

#endif // ORDER_PLACEMENT_H