#include "order_placement.h"
#include "env_handler.h"
#include <openssl/hmac.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>

OrderPlacement::OrderPlacement() : running(false)
{
    apiKey = EnvHandler::getEnvVariable("DERIBIT_API_KEY");
    apiSecret = EnvHandler::getEnvVariable("DERIBIT_API_SECRET");
    baseUrl = EnvHandler::getEnvVariable("DERIBIT_BASE_URL");

    if (apiKey.empty() || apiSecret.empty())
    {
        throw std::runtime_error("API credentials not found in environment");
    }

    setupAuth();
    authenticate();
    startWorker();
}

OrderPlacement::~OrderPlacement()
{
    stopWorker();
}

void OrderPlacement::setupAuth()
{
    client.setHeader("Content-Type", "application/json");
}

std::string OrderPlacement::sign(const std::string &message)
{
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen;

    HMAC(EVP_sha256(),
         apiSecret.c_str(), apiSecret.length(),
         reinterpret_cast<const unsigned char *>(message.c_str()), message.length(),
         hash, &hashLen);

    std::stringstream ss;
    for (unsigned int i = 0; i < hashLen; i++)
    {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }

    return ss.str();
}

void OrderPlacement::startWorker()
{
    running = true;
    workerThread = std::thread(&OrderPlacement::processRequests, this);
}

void OrderPlacement::stopWorker()
{
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        running = false;
    }
    queueCV.notify_one();
    if (workerThread.joinable())
    {
        workerThread.join();
    }
}

void OrderPlacement::processRequests()
{
    while (running)
    {
        std::unique_ptr<ApiRequest> request;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCV.wait(lock, [this]
                         { return !requestQueue.empty() || !running; });

            if (!running && requestQueue.empty())
            {
                std::cout << "Shutting down process thread" << std::endl;
                break;
            }

            if (!requestQueue.empty())
            {
                request = std::move(requestQueue.front());
                requestQueue.pop();
            }
        }

        if (request)
        {
            try
            {
                json response = sendAuthenticatedRequest(request->method, request->params);
                request->promise.set_value(response);
            }
            catch (const std::exception &e)
            {
                std::cout << "Request failed: " << e.what() << std::endl;
                request->promise.set_exception(std::current_exception());
            }
        }
    }
}

void OrderPlacement::authenticate()
{
    json authParams = {
        {"grant_type", "client_credentials"},
        {"client_id", apiKey},
        {"client_secret", apiSecret}};

    json request = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", "public/auth"},
        {"params", authParams}};

    // Clear existing auth headers for this request
    client.setHeader("Authorization", "");
    std::string fullUrl = baseUrl + "/api/v2";
    std::string response = client.post(fullUrl, request.dump());
    json responseJson = json::parse(response);

    if (responseJson.contains("result"))
    {
        access_token = responseJson["result"]["access_token"];
        refresh_token = responseJson["result"]["refresh_token"];
        token_expiry = responseJson["result"]["expires_in"];
        last_auth_time = std::chrono::steady_clock::now();
    }
    else
    {
        throw std::runtime_error("Authentication failed: " + response);
    }
}

json OrderPlacement::sendAuthenticatedRequest(const std::string &method,
                                              const json &params)
{
    auto current_time = std::chrono::steady_clock::now();
    auto time_elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_auth_time).count();

    if (time_elapsed > (token_expiry - 60))
    {
        authenticate();
    }

    json request = {
        {"jsonrpc", "2.0"},
        {"method", method},
        {"params", params},
        {"id", std::chrono::system_clock::now().time_since_epoch().count() / 1000000}};

    client.setHeader("Authorization", "Bearer " + access_token);
    std::string fullUrl = baseUrl + "/api/v2/" + method;
    std::string response = client.post(fullUrl, request.dump());
    return json::parse(response);
}

std::future<json> OrderPlacement::queueRequest(const std::string &method, const json &params)
{
    auto request = std::make_unique<ApiRequest>();
    request->method = method;
    request->params = params;
    request->timestamp = std::chrono::steady_clock::now(); // Add timestamp to request

    std::future<json> future = request->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queueMutex);
        requestQueue.push(std::move(request));
    }
    queueCV.notify_one();

    // Return the future which can be waited with timeout
    return future;
}

std::future<json> OrderPlacement::placeOrder(const std::string &instrument,
                                             const std::string &side,
                                             const std::string &type,
                                             double amount,
                                             double price,
                                             bool reduceOnly)
{
    if (side != "buy" && side != "sell")
    {
        throw std::invalid_argument("Invalid side. Must be 'buy' or 'sell'");
    }

    json params = {
        {"instrument_name", instrument},
        {"amount", amount},
        {"type", type}};

    if (type == "limit" || type == "stop_limit")
    {
        params["price"] = price;
    }

    if (type == "stop_market" || type == "stop_limit")
    {
        params["trigger"] = "last_price";
        params["trigger_price"] = price;
    }

    if (reduceOnly)
    {
        params["reduce_only"] = true;
    }

    return queueRequest("private/" + side, params);
}

std::future<json> OrderPlacement::cancelOrder(const std::string &orderId)
{
    json params = {
        {"order_id", orderId}};

    return queueRequest("private/cancel", params);
}

std::future<json> OrderPlacement::modifyOrder(const std::string &orderId,
                                              double newPrice,
                                              double newAmount)
{
    json params = {
        {"order_id", orderId},
        {"amount", newAmount},
        {"price", newPrice}};

    return queueRequest("private/edit", params);
}

std::future<json> OrderPlacement::getActiveOrders()
{
    json params = {};

    params["type"] = "all";

    return queueRequest("private/get_open_orders", params);
}

std::future<json> OrderPlacement::getOrderState(const std::string &orderId)
{
    json params = {
        {"order_id", orderId}};

    return queueRequest("private/get_order_state", params);
}

std::future<json> OrderPlacement::getOrderbook(const std::string &instrument)
{
    json params = {
        {"instrument_name", instrument},
        {"depth", 1}};

    return queueRequest("public/get_order_book", params);
}

std::future<json> OrderPlacement::getInstrumentDetails(const std::string &instrument_name)
{
    json params = {
        {"instrument_name", instrument_name}};

    return queueRequest("public/get_instrument", params);
}

std::future<json> OrderPlacement::getInstruments(const std::string &currency, const std::string &kind)
{
    json params = {
        {"currency", currency},
        {"kind", kind},
        {"expired", false}};

    return queueRequest("public/get_instruments", params);
}

std::future<json> OrderPlacement::getPositions(const std::string &currency)
{
    json params = {
        {"currency", currency}};

    return queueRequest("private/get_positions", params);
}

void OrderPlacement::printResponse(const json &response, ResponseType type, const std::string &extraInfo)
{
    if (!response.contains("result"))
    {
        std::cout << "Raw response:" << std::endl;
        std::cout << response.dump(2) << std::endl;
        return;
    }

    const auto &result = response["result"];

    switch (type)
    {
    case ResponseType::POSITION:
        if (result.is_array())
        {
            if (result.empty())
            {
                std::cout << "No positions found for " << extraInfo << std::endl;
                return;
            }
            std::cout << "\nPositions for " << extraInfo << ":" << std::endl;
            for (const auto &position : result)
            {
                printPositionDetails(position);
            }
        }
        break;

    case ResponseType::ACTIVE_ORDERS:
        if (result.is_array())
        {
            if (result.empty())
            {
                std::cout << "No active orders found." << std::endl;
                return;
            }
            std::cout << "\nActive Orders:" << std::endl;
            for (const auto &order : result)
            {
                printOrderDetails(order);
            }
        }
        break;

    case ResponseType::CANCELLED_ORDER:
        std::cout << "\nCancelled Order Details:" << std::endl;
        printOrderDetails(result, true); // true for cancelled order
        break;

    case ResponseType::ORDER_RESPONSE:
    case ResponseType::MODIFIED_ORDER:
        if (result.contains("order"))
        {
            std::cout << "\n"
                      << (type == ResponseType::MODIFIED_ORDER ? "Modified " : "")
                      << "Order Details:" << std::endl;
            printOrderDetails(result["order"]);

            if (result.contains("trades") && result["trades"].is_array() && !result["trades"].empty())
            {
                std::cout << "\nTrade Details:" << std::endl;
                for (const auto &trade : result["trades"])
                {
                    printTradeDetails(trade);
                }
            }
        }
        break;
    case ResponseType::INSTRUMENT:
        if (result.is_array())
        {
            if (result.empty())
            {
                std::cout << "No instruments found." << std::endl;
                return;
            }
            std::cout << "\nInstrument Details:" << std::endl;
            for (const auto &instrument : result)
            {
                printInstrumentDetails(instrument);
            }
        }
        else
        {
            // Single instrument case
            printInstrumentDetails(result);
        }
        break;
    case ResponseType::ORDERBOOK:
        printOrderbookDetails(result);
        break;
    default:
        std::cout << "Raw response:" << std::endl;
        std::cout << response.dump(2) << std::endl;
        break;
    }
}

void OrderPlacement::printInstrumentDetails(const json &instrument)
{
    std::cout << "----------------------------------------" << std::endl;
    if (instrument.contains("instrument_name"))
        std::cout << "Instrument Name: " << instrument["instrument_name"] << std::endl;
    if (instrument.contains("kind"))
        std::cout << "Kind: " << instrument["kind"] << std::endl;
    if (instrument.contains("base_currency"))
        std::cout << "Base Currency: " << instrument["base_currency"] << std::endl;
    if (instrument.contains("quote_currency"))
        std::cout << "Quote Currency: " << instrument["quote_currency"] << std::endl;
    if (instrument.contains("min_trade_amount"))
        std::cout << "Min Trade Amount: " << instrument["min_trade_amount"] << std::endl;
    if (instrument.contains("tick_size"))
        std::cout << "Tick Size: " << instrument["tick_size"] << std::endl;
    if (instrument.contains("is_active"))
        std::cout << "Is Active: " << (instrument["is_active"].get<bool>() ? "Yes" : "No") << std::endl;
    if (instrument.contains("creation_timestamp"))
        std::cout << "Creation Timestamp: " << instrument["creation_timestamp"] << std::endl;
    std::cout << "----------------------------------------" << std::endl;
}

void OrderPlacement::printOrderDetails(const json &order, bool isCancelled)
{
    std::cout << "----------------------------------------" << std::endl;
    if (order.contains("instrument_name"))
        std::cout << "Instrument: " << order["instrument_name"] << std::endl;
    if (order.contains("order_id"))
        std::cout << "Order ID: " << order["order_id"] << std::endl;
    if (order.contains("order_state"))
        std::cout << "State: " << order["order_state"] << std::endl;
    if (isCancelled && order.contains("cancel_reason"))
        std::cout << "Cancel Reason: " << order["cancel_reason"] << std::endl;
    if (order.contains("direction"))
        std::cout << "Direction: " << order["direction"] << std::endl;
    if (order.contains("order_type"))
        std::cout << "Type: " << order["order_type"] << std::endl;
    if (order.contains("price"))
        std::cout << "Price: " << order["price"] << std::endl;
    if (order.contains("amount"))
        std::cout << "Amount: " << order["amount"] << std::endl;
    if (order.contains("filled_amount"))
        std::cout << "Filled Amount: " << order["filled_amount"] << std::endl;
    if (order.contains("average_price"))
        std::cout << "Average Price: " << order["average_price"] << std::endl;
    if (order.contains("time_in_force"))
        std::cout << "Time In Force: " << order["time_in_force"] << std::endl;
    if (order.contains("post_only"))
        std::cout << "Post Only: " << (order["post_only"].get<bool>() ? "Yes" : "No") << std::endl;
    if (order.contains("creation_timestamp"))
        std::cout << "Created: " << order["creation_timestamp"] << std::endl;
    if (order.contains("last_update_timestamp"))
        std::cout << "Last Update: " << order["last_update_timestamp"] << std::endl;
}

void OrderPlacement::printTradeDetails(const json &trade)
{
    std::cout << "----------------------------------------" << std::endl;
    if (trade.contains("trade_id"))
        std::cout << "Trade ID: " << trade["trade_id"] << std::endl;
    if (trade.contains("price"))
        std::cout << "Trade Price: " << trade["price"] << std::endl;
    if (trade.contains("amount"))
        std::cout << "Trade Amount: " << trade["amount"] << std::endl;
    if (trade.contains("fee"))
        std::cout << "Fee: " << trade["fee"] << std::endl;
    if (trade.contains("fee_currency"))
        std::cout << "Fee Currency: " << trade["fee_currency"] << std::endl;
    if (trade.contains("mark_price"))
        std::cout << "Mark Price: " << trade["mark_price"] << std::endl;
    if (trade.contains("index_price"))
        std::cout << "Index Price: " << trade["index_price"] << std::endl;
    if (trade.contains("state"))
        std::cout << "Trade State: " << trade["state"] << std::endl;
    if (trade.contains("timestamp"))
        std::cout << "Trade Time: " << trade["timestamp"] << std::endl;
}

void OrderPlacement::printPositionDetails(const json &position)
{
    std::cout << "----------------------------------------" << std::endl;
    if (position.contains("instrument_name"))
        std::cout << "Instrument: " << position["instrument_name"] << std::endl;
    if (position.contains("size"))
        std::cout << "Size: " << position["size"] << std::endl;
    if (position.contains("direction"))
        std::cout << "Direction: " << position["direction"] << std::endl;
    if (position.contains("average_price"))
        std::cout << "Average Price: " << position["average_price"] << std::endl;
    if (position.contains("floating_profit_loss"))
        std::cout << "Floating P/L: " << position["floating_profit_loss"] << std::endl;
    if (position.contains("mark_price"))
        std::cout << "Mark Price: " << position["mark_price"] << std::endl;
    if (position.contains("leverage"))
        std::cout << "Leverage: " << position["leverage"] << std::endl;
    if (position.contains("maintenance_margin"))
        std::cout << "Maintenance Margin: " << position["maintenance_margin"] << std::endl;
    if (position.contains("initial_margin"))
        std::cout << "Initial Margin: " << position["initial_margin"] << std::endl;
    if (position.contains("liquidation_price"))
        std::cout << "Liquidation Price: " << position["liquidation_price"] << std::endl;
}

void OrderPlacement::printOrderbookDetails(const json &orderbook)
{
    std::cout << "----------------------------------------" << std::endl;
    if (orderbook.contains("instrument_name"))
        std::cout << "Instrument: " << orderbook["instrument_name"] << std::endl;

    std::cout << "\nBest Prices:" << std::endl;
    if (orderbook.contains("best_bid_price") && orderbook.contains("best_bid_amount"))
        std::cout << "Best Bid: " << orderbook["best_bid_price"] << " ("
                  << orderbook["best_bid_amount"] << ")" << std::endl;
    if (orderbook.contains("best_ask_price") && orderbook.contains("best_ask_amount"))
        std::cout << "Best Ask: " << orderbook["best_ask_price"] << " ("
                  << orderbook["best_ask_amount"] << ")" << std::endl;

    std::cout << "\nMarket Prices:" << std::endl;
    if (orderbook.contains("last_price"))
        std::cout << "Last Price: " << orderbook["last_price"] << std::endl;
    if (orderbook.contains("mark_price"))
        std::cout << "Mark Price: " << orderbook["mark_price"] << std::endl;
    if (orderbook.contains("index_price"))
        std::cout << "Index Price: " << orderbook["index_price"] << std::endl;

    if (orderbook.contains("stats"))
    {
        const auto &stats = orderbook["stats"];
        std::cout << "\nTrading Stats:" << std::endl;
        if (stats.contains("high"))
            std::cout << "24h High: " << stats["high"] << std::endl;
        if (stats.contains("low"))
            std::cout << "24h Low: " << stats["low"] << std::endl;
        if (stats.contains("price_change"))
            std::cout << "Price Change: " << stats["price_change"] << "%" << std::endl;
        if (stats.contains("volume"))
            std::cout << "Volume: " << stats["volume"] << std::endl;
        if (stats.contains("volume_notional"))
            std::cout << "Volume Notional: " << stats["volume_notional"] << std::endl;
    }

    std::cout << "\nOrderbook Depth:" << std::endl;
    if (orderbook.contains("bids") && orderbook["bids"].is_array())
    {
        std::cout << "Bids:" << std::endl;
        for (const auto &bid : orderbook["bids"])
        {
            if (bid.is_array() && bid.size() >= 2)
            {
                std::cout << "  Price: " << bid[0] << " | Amount: " << bid[1] << std::endl;
            }
        }
    }

    if (orderbook.contains("asks") && orderbook["asks"].is_array())
    {
        std::cout << "Asks:" << std::endl;
        for (const auto &ask : orderbook["asks"])
        {
            if (ask.is_array() && ask.size() >= 2)
            {
                std::cout << "  Price: " << ask[0] << " | Amount: " << ask[1] << std::endl;
            }
        }
    }

    if (orderbook.contains("state"))
        std::cout << "\nMarket State: " << orderbook["state"] << std::endl;
    if (orderbook.contains("timestamp"))
        std::cout << "Timestamp: " << orderbook["timestamp"] << std::endl;

    std::cout << "----------------------------------------" << std::endl;
}