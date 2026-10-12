#pragma once

#include <any>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace soilwatercm::infrastructure
{

    /**
     * @brief 全局配置单例
     *
     * 键值扁平存储 (如 "server.port"), 支持热更新回调。
     */
    class ConfigManager
    {
    public:
        /// @brief 全局唯一实例
        static ConfigManager& instance();

        /// @brief instance() 的别名
        static ConfigManager& getInstance() { return instance(); }

        /**
         * @brief 读取 YAML 配置文件并解析进内存
         * @param config_file 配置文件路径; 打不开时抛异常
         */
        void load(const std::string& config_file);

        /// @brief 载入内置默认值 (server/postgresql/mongodb/redis/storage)
        void loadDefaults();

        /**
         * @brief 按键取值
         * @param key 配置键
         * @param default_value 缺省值
         * @return 配置值; 类型不符或不存在返回 default_value
         */
        template <typename T>
        T get(const std::string& key, const T& default_value = T{}) const;

        /// @brief 写入配置项
        template <typename T>
        void set(const std::string& key, const T& value);

        /// @brief 键是否存在
        bool has(const std::string& key) const;

        /// @brief 读环境变量, 不存在返回默认值
        std::string getEnvVar(const std::string& key, const std::string& default_value = "") const;

        /// @brief 注册某配置键的热更新回调
        void setHotUpdateCallback(const std::string& key, std::function<void()> callback);

        /// @brief 触发某配置键已注册的热更新回调
        void triggerHotUpdate(const std::string& key);

        /// @brief 重新读取上次 load 的配置文件
        void reload();

        // ---- 常用配置快捷读取 ----

        /// @brief 服务监听地址
        std::string getServerHost() const;

        /// @brief 服务端口
        int getServerPort() const;

        /// @brief 服务工作线程数
        int getServerWorkers() const;

        /// @brief PostgreSQL 主机
        std::string getPostgresHost() const;

        /// @brief PostgreSQL 端口
        int getPostgresPort() const;

        /// @brief PostgreSQL 库名
        std::string getPostgresDatabase() const;

        /// @brief PostgreSQL 用户名
        std::string getPostgresUser() const;

        /// @brief PostgreSQL 密码
        std::string getPostgresPassword() const;

        /// @brief MongoDB 主机
        std::string getMongoHost() const;

        /// @brief MongoDB 端口
        int getMongoPort() const;

        /// @brief MongoDB 库名
        std::string getMongoDatabase() const;

        /// @brief Redis 主机
        std::string getRedisHost() const;

        /// @brief Redis 端口
        int getRedisPort() const;

        /// @brief 存储根目录
        std::string getStorageBaseDir() const;

        /// @brief 原始数据目录
        std::string getStorageRawDir() const;

        /// @brief 处理后数据目录
        std::string getStorageProcessedDir() const;

        /// @brief 结果数据目录
        std::string getStorageResultsDir() const;

        /// @brief 取某数据库前缀下的全部配置, 值统一转成字符串
        std::unordered_map<std::string, std::string> getDatabaseConfig(const std::string& dbType) const;

        /// @brief 限流相关配置
        std::unordered_map<std::string, std::string> getRateLimitConfig() const;

        ConfigManager(const ConfigManager&) = delete;
        ConfigManager& operator=(const ConfigManager&) = delete;
        ConfigManager(ConfigManager&&) = delete;
        ConfigManager& operator=(ConfigManager&&) = delete;

    private:
        ConfigManager();
        ~ConfigManager() = default;

        void parseYaml(const std::string& content);
        std::string resolveEnvVars(const std::string& value) const;

        std::unordered_map<std::string, std::any> config_values_;
        std::unordered_map<std::string, std::function<void()>> hot_update_callbacks_;
        mutable std::mutex mutex_;
        std::string config_file_;
    };

    template <typename T>
    T ConfigManager::get(const std::string& key, const T& default_value) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = config_values_.find(key);
        if (it != config_values_.end())
        {
            try
            {
                return std::any_cast<T>(it->second);
            }
            catch (const std::bad_any_cast&)
            {
                return default_value;
            }
        }
        return default_value;
    }

    template <typename T>
    void ConfigManager::set(const std::string& key, const T& value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        config_values_[key] = value;
    }

}  // namespace soilwatercm::infrastructure
