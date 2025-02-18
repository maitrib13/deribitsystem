#include "env_handler.h"
#include <fstream>
#include <iostream>
#include <sstream>

std::map<std::string, std::string> EnvHandler::envVars;

bool EnvHandler::loadEnvFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open " << filename << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Find the position of '='
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            // Remove quotes if present
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }

            envVars[key] = value;
        }
    }

    return true;
}

std::string EnvHandler::getEnvVariable(const std::string& key) {
    auto it = envVars.find(key);
    if (it != envVars.end()) {
        return it->second;
    }
    
    // Try to get from system environment
    const char* val = std::getenv(key.c_str());
    return val ? std::string(val) : "";
}