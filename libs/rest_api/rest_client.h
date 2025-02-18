// rest_client.h
#ifndef REST_CLIENT_H
#define REST_CLIENT_H

#include <string>
#include <map>
#include <memory>
#include <stdexcept>

// Forward declaration
struct curl_slist;

class RestClient {
public:
    /**
     * @brief Construct a new RestClient object
     * 
     */
    RestClient();
    
    /**
     * @brief Destroy the RestClient object
     * 
     */
    ~RestClient();

    /**
     * @brief Perform a GET request
     * 
     * @param url The URL to send the request to
     * @return std::string The response from the server
     */
    std::string get(const std::string& url);
    
    /**
     * @brief Perform a POST request
     * 
     * @param url The URL to send the request to
     * @param payload The payload to send in the request
     * @return std::string The response from the server
     */
    std::string post(const std::string& url, const std::string& payload);
    
    /**
     * @brief Perform a PUT request
     * 
     * @param url The URL to send the request to
     * @param payload The payload to send in the request
     * @return std::string The response from the server
     */
    std::string put(const std::string& url, const std::string& payload);
    
    /**
     * @brief Perform a DELETE request
     * 
     * @param url The URL to send the request to
     * @return std::string The response from the server
     */
    std::string del(const std::string& url); // 'delete' is a reserved word

    /**
     * @brief Set a header for the request
     * 
     * @param key The header key
     * @param value The header value
     */
    void setHeader(const std::string& key, const std::string& value);
    
    /**
     * @brief Set the timeout for the request
     * 
     * @param seconds The timeout in seconds
     */
    void setTimeout(long seconds);
    
    /**
     * @brief Set whether to verify SSL certificates
     * 
     * @param verify Whether to verify SSL certificates
     */
    void setVerifySsl(bool verify);
    
    /**
     * @brief Get the last response code
     * 
     * @return int The last response code
     */
    int getLastResponseCode() const;
    
    /**
     * @brief Get the last error message
     * 
     * @return std::string The last error message
     */
    std::string getLastErrorMessage() const;
    
    /**
     * @brief Exception class for RestClient
     * 
     */
    class Exception : public std::runtime_error {
    public:
        /**
         * @brief Construct a new Exception object
         * 
         * @param message The exception message
         */
        explicit Exception(const std::string& message) : std::runtime_error(message) {}
    };

private:
    // Private implementation details
    class Impl;
    std::unique_ptr<Impl> pimpl;

    // Prevent copying
    RestClient(const RestClient&) = delete;
    RestClient& operator=(const RestClient&) = delete;
};

#endif // REST_CLIENT_H