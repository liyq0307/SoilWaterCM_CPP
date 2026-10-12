#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <hiredis/hiredis.h>

namespace soilwatercm::repository
{

    /// Redis 连接参数
    struct RedisConfig
    {
        std::string host = "localhost";
        int port = 6379;
        std::string password = "";
        int database = 0;
        int connection_timeout = 5;
        int socket_timeout = 3;
    };

    /**
     * @brief hiredis 回复对象的 RAII 包装
     *
     * 负责释放 redisReply, 并提供类型判断与取值。
     */
    class RedisReply
    {
    public:
        /**
         * @brief 构造, 接管回复对象所有权
         * @param reply hiredis 回复 (可为 nullptr)
         */
        explicit RedisReply(redisReply* reply);
        ~RedisReply();

        /// @brief 是否 OK 状态回复
        bool isOk() const;

        /// @brief 是否空回复 (键不存在等)
        bool isNil() const;

        /// @brief 是否字符串回复
        bool isString() const;

        /// @brief 是否整数回复
        bool isInteger() const;

        /// @brief 是否数组回复
        bool isArray() const;

        /// @brief 取字符串值
        std::string asString() const;

        /// @brief 取整数值
        int64_t asInteger() const;

        /// @brief 取数组全部元素 (转字符串)
        std::vector<std::string> asArray() const;

        /// @brief 数组元素个数
        size_t arraySize() const;

        /// @brief 取数组第 index 个元素 (浅引用, 勿在 reply 释放后使用)
        RedisReply elementAt(size_t index) const;

        RedisReply(const RedisReply&) = delete;
        RedisReply& operator=(const RedisReply&) = delete;
        RedisReply(RedisReply&&) noexcept;
        RedisReply& operator=(RedisReply&&) noexcept;

    private:
        redisReply* reply_;
    };

    /**
     * @brief Redis 客户端封装
     *
     * 字符串/哈希/列表常用命令, 以及任意命令的透传执行。
     */
    class RedisRepository
    {
    public:
        /**
         * @brief 构造, 暂不连接
         * @param config 连接参数
         */
        explicit RedisRepository(const RedisConfig& config);
        ~RedisRepository();

        /// @brief 建立连接并认证/选库; 已连接直接返回 true
        bool connect();

        /// @brief 断开连接
        void disconnect();

        /// @brief 是否已连接
        bool isConnected() const;

        // 键值操作, expire_seconds 大于 0 时设置过期

        /// @brief GET, 键不存在返回 nullopt
        std::optional<std::string> get(const std::string& key);

        /// @brief SET/SETEX
        bool set(const std::string& key, const std::string& value, int expire_seconds = 0);

        /// @brief DEL, 确有删除返回 true
        bool del(const std::string& key);

        /// @brief EXPIRE
        bool expire(const std::string& key, int seconds);

        /// @brief TTL, 无过期或异常返回 -1
        int64_t ttl(const std::string& key);

        /// @brief EXISTS
        bool exists(const std::string& key);

        /// @brief MGET, 不存在的键对应 nullopt
        std::vector<std::optional<std::string>> mget(const std::vector<std::string>& keys);

        /// @brief MSET 批量写入
        bool mset(const std::vector<std::pair<std::string, std::string>>& kv_pairs);

        /// @brief INCR, 返回自增后的值
        int64_t incr(const std::string& key);

        /// @brief INCRBY
        int64_t incrBy(const std::string& key, int64_t increment);

        /// @brief DECR
        int64_t decr(const std::string& key);

        /// @brief HSET 单字段
        bool hset(const std::string& name, const std::string& key, const std::string& value);

        /// @brief HGET, 字段不存在返回 nullopt
        std::optional<std::string> hget(const std::string& name, const std::string& key);

        /// @brief HDEL
        bool hdel(const std::string& name, const std::string& key);

        /// @brief HKEYS 全部字段名
        std::vector<std::string> hkeys(const std::string& name);

        /// @brief HVALS 全部字段值
        std::vector<std::string> hvals(const std::string& name);

        /// @brief LPUSH, 返回列表长度
        int64_t lpush(const std::string& key, const std::string& value);

        /// @brief RPUSH, 返回列表长度
        int64_t rpush(const std::string& key, const std::string& value);

        /// @brief LPOP, 空列表返回 nullopt
        std::optional<std::string> lpop(const std::string& key);

        /// @brief RPOP, 空列表返回 nullopt
        std::optional<std::string> rpop(const std::string& key);

        /// @brief LRANGE 区间元素
        std::vector<std::string> lrange(const std::string& key, int64_t start, int64_t stop);

        /**
         * @brief 透传执行任意命令 (hiredis 格式化参数)
         * @param format 命令格式串, 如 "GET %s"
         * @return 回复包装; 未连接返回空回复
         */
        RedisReply executeCommand(const char* format, ...);

        RedisRepository(const RedisRepository&) = delete;
        RedisRepository& operator=(const RedisRepository&) = delete;
        RedisRepository(RedisRepository&&) noexcept;
        RedisRepository& operator=(RedisRepository&&) noexcept;

    private:
        RedisReply executeCommandArgs(int argc, const char** argv, const size_t* argvlen);

        RedisConfig config_;
        redisContext* context_;
    };

}  // namespace soilwatercm::repository
