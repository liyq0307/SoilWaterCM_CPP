#define _CRT_SECURE_NO_WARNINGS
#include "config_manager.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace soilwatercm::infrastructure
{

    ConfigManager& ConfigManager::instance()
    {
        static ConfigManager instance;
        return instance;
    }

    ConfigManager::ConfigManager()
    {
        loadDefaults();
    }

    void ConfigManager::load(const std::string& config_file)
    {
        config_file_ = config_file;

        std::ifstream file(config_file);
        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open config file: " + config_file);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        parseYaml(content);
    }

    void ConfigManager::loadDefaults()
    {
        set("server.host", std::string("0.0.0.0"));
        set("server.port", 8080);
        set("server.workers", 4);

        set("postgresql.host", std::string("localhost"));
        set("postgresql.port", 5432);
        set("postgresql.database", std::string("soilwatercm"));
        set("postgresql.user", std::string("postgres"));
        set("postgresql.password", std::string(""));

        set("mongodb.host", std::string("localhost"));
        set("mongodb.port", 27017);
        set("mongodb.database", std::string("soilwatercm_meta"));

        set("redis.host", std::string("localhost"));
        set("redis.port", 6379);

        set("storage.base_dir", std::string("data"));
        set("storage.raw_dir", std::string("data/raw"));
        set("storage.processed_dir", std::string("data/processed"));
        set("storage.results_dir", std::string("data/results"));
    }

    bool ConfigManager::has(const std::string& key) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_values_.find(key) != config_values_.end();
    }

    std::string ConfigManager::getEnvVar(const std::string& key, const std::string& default_value) const
    {
        const char* value = std::getenv(key.c_str());
        return value ? std::string(value) : default_value;
    }

    void ConfigManager::setHotUpdateCallback(const std::string& key, std::function<void()> callback)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        hot_update_callbacks_[key] = callback;
    }

    void ConfigManager::triggerHotUpdate(const std::string& key)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = hot_update_callbacks_.find(key);
        if (it != hot_update_callbacks_.end())
        {
            it->second();
        }
    }

    void ConfigManager::reload()
    {
        if (!config_file_.empty())
        {
            load(config_file_);
        }
    }

    std::string ConfigManager::getServerHost() const
    {
        return get<std::string>("server.host", "0.0.0.0");
    }

    int ConfigManager::getServerPort() const
    {
        return get<int>("server.port", 8080);
    }

    int ConfigManager::getServerWorkers() const
    {
        return get<int>("server.workers", 4);
    }

    std::string ConfigManager::getPostgresHost() const
    {
        std::string host = get<std::string>("postgresql.host", "localhost");
        return resolveEnvVars(host);
    }

    int ConfigManager::getPostgresPort() const
    {
        return get<int>("postgresql.port", 5432);
    }

    std::string ConfigManager::getPostgresDatabase() const
    {
        return get<std::string>("postgresql.database", "soilwatercm");
    }

    std::string ConfigManager::getPostgresUser() const
    {
        std::string user = get<std::string>("postgresql.user", "postgres");
        return resolveEnvVars(user);
    }

    std::string ConfigManager::getPostgresPassword() const
    {
        std::string password = get<std::string>("postgresql.password", "");
        return resolveEnvVars(password);
    }

    std::string ConfigManager::getMongoHost() const
    {
        std::string host = get<std::string>("mongodb.host", "localhost");
        return resolveEnvVars(host);
    }

    int ConfigManager::getMongoPort() const
    {
        return get<int>("mongodb.port", 27017);
    }

    std::string ConfigManager::getMongoDatabase() const
    {
        return get<std::string>("mongodb.database", "soilwatercm_meta");
    }

    std::string ConfigManager::getRedisHost() const
    {
        std::string host = get<std::string>("redis.host", "localhost");
        return resolveEnvVars(host);
    }

    int ConfigManager::getRedisPort() const
    {
        return get<int>("redis.port", 6379);
    }

    std::string ConfigManager::getStorageBaseDir() const
    {
        return get<std::string>("storage.base_dir", "data");
    }

    std::string ConfigManager::getStorageRawDir() const
    {
        return get<std::string>("storage.raw_dir", "data/raw");
    }

    std::string ConfigManager::getStorageProcessedDir() const
    {
        return get<std::string>("storage.processed_dir", "data/processed");
    }

    std::string ConfigManager::getStorageResultsDir() const
    {
        return get<std::string>("storage.results_dir", "data/results");
    }

    std::unordered_map<std::string, std::string> ConfigManager::getDatabaseConfig(const std::string& dbType) const
    {
        std::unordered_map<std::string, std::string> config;
        std::string prefix = dbType + ".";

        for (const auto& [key, value] : config_values_)
        {
            if (key.find(prefix) == 0)
            {
                std::string subKey = key.substr(prefix.size());
                try
                {
                    config[subKey] = std::any_cast<std::string>(value);
                }
                catch (const std::bad_any_cast&)
                {
                    try
                    {
                        config[subKey] = std::to_string(std::any_cast<int>(value));
                    }
                    catch (const std::bad_any_cast&)
                    {
                        try
                        {
                            config[subKey] = std::to_string(std::any_cast<double>(value));
                        }
                        catch (const std::bad_any_cast&)
                        {
                            try
                            {
                                config[subKey] = std::any_cast<bool>(value) ? "true" : "false";
                            }
                            catch (const std::bad_any_cast&)
                            {
                            }
                        }
                    }
                }
            }
        }

        return config;
    }

    std::unordered_map<std::string, std::string> ConfigManager::getRateLimitConfig() const
    {
        std::unordered_map<std::string, std::string> config;

        config["max_requests"] = std::to_string(get<int>("processing.max_concurrent_tasks", 100));
        config["window_seconds"] = "60";

        for (const auto& [key, value] : config_values_)
        {
            if (key.find("rate_limit.") == 0)
            {
                std::string subKey = key.substr(11);
                try
                {
                    config[subKey] = std::any_cast<std::string>(value);
                }
                catch (const std::bad_any_cast&)
                {
                    try
                    {
                        config[subKey] = std::to_string(std::any_cast<int>(value));
                    }
                    catch (const std::bad_any_cast&)
                    {
                    }
                }
            }
        }

        return config;
    }

    void ConfigManager::parseYaml(const std::string& content)
    {
        std::istringstream iss(content);
        std::string line;
        std::string currentSection;

        while (std::getline(iss, line))
        {
            size_t firstNonSpace = line.find_first_not_of(" \t");
            if (firstNonSpace == std::string::npos)
                continue;
            line = line.substr(firstNonSpace);

            if (line.empty() || line[0] == '#')
                continue;

            size_t colonPos = line.find(':');
            if (colonPos == std::string::npos)
                continue;

            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);

            size_t lastNonSpace = value.find_last_not_of(" \t");
            if (lastNonSpace != std::string::npos)
            {
                value = value.substr(0, lastNonSpace + 1);
            }
            size_t firstNonSpaceVal = value.find_first_not_of(" \t");
            if (firstNonSpaceVal != std::string::npos)
            {
                value = value.substr(firstNonSpaceVal);
            }

            if (key.find_first_not_of(" \t") == 0 && key[0] != ' ' && value.empty())
            {
                currentSection = key;
                continue;
            }

            std::string fullKey;
            if (!currentSection.empty())
            {
                fullKey = currentSection + "." + key;
            }
            else
            {
                fullKey = key;
            }

            if (value.empty())
                continue;

            if (value.front() == '"' && value.back() == '"')
            {
                set(fullKey, value.substr(1, value.size() - 2));
            }
            else if (value.front() == '\'' && value.back() == '\'')
            {
                set(fullKey, value.substr(1, value.size() - 2));
            }
            else if (value == "true" || value == "True" || value == "TRUE")
            {
                set(fullKey, true);
            }
            else if (value == "false" || value == "False" || value == "FALSE")
            {
                set(fullKey, false);
            }
            else
            {
                try
                {
                    int intVal = std::stoi(value);
                    set(fullKey, intVal);
                }
                catch (...)
                {
                    try
                    {
                        double dblVal = std::stod(value);
                        set(fullKey, dblVal);
                    }
                    catch (...)
                    {
                        set(fullKey, value);
                    }
                }
            }
        }
    }

    std::string ConfigManager::resolveEnvVars(const std::string& value) const
    {
        if (value.empty())
        {
            return value;
        }

        if (value.front() == '$')
        {
            std::string env_var = value.substr(1);
            if (env_var.front() == '{' && env_var.back() == '}')
            {
                env_var = env_var.substr(1, env_var.length() - 2);
            }
            return getEnvVar(env_var, value);
        }

        return value;
    }

}  // namespace soilwatercm::infrastructure
