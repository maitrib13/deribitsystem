#ifndef ENV_HANDLER_H
#define ENV_HANDLER_H

#include <string>
#include <map>

class EnvHandler {
public:
    /**
     * @brief Load environment variables from a file
     * 
     * @param filename The name of the file to load (default is ".env")
     * @return true if the file was loaded successfully
     * @return false if there was an error loading the file
     */
    static bool loadEnvFile(const std::string& filename = ".env");
    
    /**
     * @brief Get the value of an environment variable
     * 
     * @param key The key of the environment variable
     * @return std::string The value of the environment variable
     */
    static std::string getEnvVariable(const std::string& key);
    
private:
    static std::map<std::string, std::string> envVars;
};

#endif // ENV_HANDLER_H