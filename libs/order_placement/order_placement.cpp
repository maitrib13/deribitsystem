// order_placement.cpp
#include "order_placement.h"
#include "env_handler.h"
#include <openssl/hmac.h>
#include <chrono>
#include <iomanip>
#include <sstream>

OrderPlacement::OrderPlacement() {
    // Load API credentials from environment
    apiKey = EnvHandler::getEnvVariable("DERIBIT_API_KEY");
    apiSecret = EnvHandler::getEnvVariable("DERIBIT_API_SECRET");
    
    if (apiKey.empty() || apiSecret.empty()) {
        throw std::runtime_error("API credentials not found in environment");
    }
    
    setupAuth();
}

void OrderPlacement::setupAuth() {
    client.setHeader("Content-Type", "application/json");
}

std::string OrderPlacement::sign(const std::string& message) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen;

    HMAC(EVP_sha256(),
         apiSecret.c_str(), apiSecret.length(),
         reinterpret_cast<const unsigned char*>(message.c_str()), message.length(),
         hash, &hashLen);

    std::stringstream ss;
    for (unsigned int i = 0; i < hashLen; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    
    return ss.str();
}

json OrderPlacement::sendAuthenticatedRequest(const std::string& method,
                                            const json& params) {
    auto timestamp = std::chrono::system_clock::now().time_since_epoch().count() / 1000000;
    
    json request = {
        {"jsonrpc", "2.0"},
        {"method", method},
        {"params", params},
        {"id", timestamp}  // Using timestamp as request ID
    };
    
    std::string signature = sign(request.dump());
    client.setHeader("Authorization", "deribit_sign " + apiKey + ":" + signature);
    
    std::string response = client.post("https://test.deribit.com/api/v2", request.dump());
    return json::parse(response);
}

json OrderPlacement::placeOrder(const std::string& instrument,
                              const std::string& side,
                              const std::string& type,
                              double amount,
                              double price,
                              bool reduceOnly) {
    // Validate side
    if (side != "buy" && side != "sell") {
        throw std::invalid_argument("Invalid side. Must be 'buy' or 'sell'");
    }

    json params = {
        {"instrument_name", instrument},
        {"amount", amount},
        {"type", type}
    };

    if (type == "limit" || type == "stop_limit") {
        params["price"] = price;
    }
    
    if (type == "stop_market" || type == "stop_limit") {
        params["trigger"] = "last_price";
        params["trigger_price"] = price;
    }

    if (reduceOnly) {
        params["reduce_only"] = true;
    }

    // Method is private/buy or private/sell
    return sendAuthenticatedRequest("private/" + side, params);
}

json OrderPlacement::cancelOrder(const std::string& orderId) {
    json params = {
        {"order_id", orderId}
    };
    
    return sendAuthenticatedRequest("private/cancel", params);
}

json OrderPlacement::modifyOrder(const std::string& orderId,
                               double newPrice,
                               double newAmount) {
    json params = {
        {"order_id", orderId},
        {"amount", newAmount},
        {"price", newPrice}
    };
    
    return sendAuthenticatedRequest("private/edit", params);
}

json OrderPlacement::getActiveOrders(const std::string& instrument) {
    json params = {};
    if (!instrument.empty()) {
        params["instrument_name"] = instrument;
    }
    
    return sendAuthenticatedRequest("private/get_open_orders_by_instrument", params);
}

json OrderPlacement::getOrderState(const std::string& orderId) {
    json params = {
        {"order_id", orderId}
    };
    
    return sendAuthenticatedRequest("private/get_order_state", params);
}

json OrderPlacement::getOrderbook(const std::string& instrument) {
    json params = {
        {"instrument_name", instrument},
        {"depth", 1}
    };
    
    return sendAuthenticatedRequest("public/get_order_book", params);
}

json OrderPlacement::getInstruments(const std::string& currency, const std::string& kind) {
    json params = {
        {"currency", currency},
        {"kind", kind},
        {"expired", false}
    };
    
    return sendAuthenticatedRequest("public/get_instruments", params);
}

json OrderPlacement::getPositions(const std::string& currency) {
    json params = {
        {"currency", currency}
    };
    
    return sendAuthenticatedRequest("private/get_positions", params);
}