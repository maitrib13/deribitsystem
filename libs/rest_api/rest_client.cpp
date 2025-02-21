// rest_client.cpp
#include "rest_client.h"
#include <curl/curl.h>
#include <sstream>

// Private implementation class
class RestClient::Impl {
public:
    Impl() : curl(nullptr), headers(nullptr), lastResponseCode(0) {
        curl = curl_easy_init();
        if (!curl) {
            throw RestClient::Exception("Failed to initialize CURL");
        }
    }

    ~Impl() {
        if (headers) {
            curl_slist_free_all(headers);
        }
        if (curl) {
            curl_easy_cleanup(curl);
        }
    }

    CURL* curl;
    struct curl_slist* headers;
    std::string lastError;
    long lastResponseCode;
    std::map<std::string, std::string> headerMap;

    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        userp->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    void updateHeaders() {
        if (headers) {
            curl_slist_free_all(headers);
            headers = nullptr;
        }

        for (const auto& header : headerMap) {
            std::string headerString = header.first + ": " + header.second;
            headers = curl_slist_append(headers, headerString.c_str());
        }

        if (headers) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        }
    }

    std::string performRequest() {
        std::string response;
        std::string errorBuffer(CURL_ERROR_SIZE, '\0');

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, &errorBuffer[0]);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            lastError = errorBuffer;
            std::stringstream ss;
            ss << "CURL error: " << curl_easy_strerror(res);
            if (!lastError.empty()) {
                ss << " (" << lastError << ")";
            }
            throw RestClient::Exception(ss.str());
        }

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &lastResponseCode);
        return response;
    }
};

// RestClient implementation
RestClient::RestClient() : pimpl(std::make_unique<Impl>()) {
    // Set default headers
    setHeader("Accept", "application/json");
    setHeader("Content-Type", "application/json");
}

RestClient::~RestClient() = default;

std::string RestClient::get(const std::string& url) {
    curl_easy_setopt(pimpl->curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(pimpl->curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(pimpl->curl, CURLOPT_CONNECTTIMEOUT, 10L);  
    setTimeout(30);  // Default 30 seconds if not set
    pimpl->updateHeaders();
    return pimpl->performRequest();
}

std::string RestClient::post(const std::string& url, const std::string& payload) {
    curl_easy_setopt(pimpl->curl, CURLOPT_POST, 1L);
    curl_easy_setopt(pimpl->curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(pimpl->curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(pimpl->curl, CURLOPT_CONNECTTIMEOUT, 10L);  
    setTimeout(30);  // Default 30 seconds if not set
    pimpl->updateHeaders();
    return pimpl->performRequest();
}

std::string RestClient::put(const std::string& url, const std::string& payload) {
    curl_easy_setopt(pimpl->curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(pimpl->curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(pimpl->curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(pimpl->curl, CURLOPT_CONNECTTIMEOUT, 10L);  
    setTimeout(30);  // Default 30 seconds if not set
    pimpl->updateHeaders();
    return pimpl->performRequest();
}

std::string RestClient::del(const std::string& url) {
    curl_easy_setopt(pimpl->curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(pimpl->curl, CURLOPT_URL, url.c_str());
    pimpl->updateHeaders();
    return pimpl->performRequest();
}

void RestClient::setHeader(const std::string& key, const std::string& value) {
    pimpl->headerMap[key] = value;
}

void RestClient::setTimeout(long seconds) {
    curl_easy_setopt(pimpl->curl, CURLOPT_TIMEOUT, seconds);
}

void RestClient::setVerifySsl(bool verify) {
    curl_easy_setopt(pimpl->curl, CURLOPT_SSL_VERIFYPEER, verify ? 1L : 0L);
    curl_easy_setopt(pimpl->curl, CURLOPT_SSL_VERIFYHOST, verify ? 2L : 0L);
}

int RestClient::getLastResponseCode() const {
    return pimpl->lastResponseCode;
}

std::string RestClient::getLastErrorMessage() const {
    return pimpl->lastError;
}