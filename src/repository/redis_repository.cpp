#include "redis_repository.h"

#include <cstdarg>
#include <cstring>
#include <stdexcept>

#include <winsock2.h>

namespace soilwatercm::repository
{

    RedisReply::RedisReply(redisReply* reply) : reply_(reply) {}

    RedisReply::~RedisReply()
    {
        if (reply_)
        {
            freeReplyObject(reply_);
        }
    }

    RedisReply::RedisReply(RedisReply&& other) noexcept : reply_(other.reply_)
    {
        other.reply_ = nullptr;
    }

    RedisReply& RedisReply::operator=(RedisReply&& other) noexcept
    {
        if (this != &other)
        {
            if (reply_)
            {
                freeReplyObject(reply_);
            }
            reply_ = other.reply_;
            other.reply_ = nullptr;
        }
        return *this;
    }

    bool RedisReply::isOk() const
    {
        return reply_ != nullptr;
    }

    bool RedisReply::isNil() const
    {
        return reply_ && reply_->type == REDIS_REPLY_NIL;
    }

    bool RedisReply::isString() const
    {
        return reply_ && reply_->type == REDIS_REPLY_STRING;
    }

    bool RedisReply::isInteger() const
    {
        return reply_ && reply_->type == REDIS_REPLY_INTEGER;
    }

    bool RedisReply::isArray() const
    {
        return reply_ && reply_->type == REDIS_REPLY_ARRAY;
    }

    std::string RedisReply::asString() const
    {
        if (isString())
        {
            return std::string(reply_->str, reply_->len);
        }
        return "";
    }

    int64_t RedisReply::asInteger() const
    {
        if (isInteger())
        {
            return reply_->integer;
        }
        return 0;
    }

    std::vector<std::string> RedisReply::asArray() const
    {
        std::vector<std::string> result;
        if (isArray())
        {
            for (size_t i = 0; i < reply_->elements; ++i)
            {
                redisReply* element = reply_->element[i];
                if (element->type == REDIS_REPLY_STRING)
                {
                    result.emplace_back(element->str, element->len);
                }
            }
        }
        return result;
    }

    size_t RedisReply::arraySize() const
    {
        if (isArray())
        {
            return reply_->elements;
        }
        return 0;
    }

    RedisReply RedisReply::elementAt(size_t index) const
    {
        if (isArray() && index < reply_->elements)
        {
            return RedisReply(static_cast<redisReply*>(reply_->element[index]));
        }
        return RedisReply(nullptr);
    }

    RedisRepository::RedisRepository(const RedisConfig& config) : config_(config), context_(nullptr) {}

    RedisRepository::~RedisRepository()
    {
        disconnect();
    }

    RedisRepository::RedisRepository(RedisRepository&& other) noexcept
        : config_(std::move(other.config_)), context_(other.context_)
    {
        other.context_ = nullptr;
    }

    RedisRepository& RedisRepository::operator=(RedisRepository&& other) noexcept
    {
        if (this != &other)
        {
            disconnect();
            config_ = std::move(other.config_);
            context_ = other.context_;
            other.context_ = nullptr;
        }
        return *this;
    }

    bool RedisRepository::connect()
    {
        if (context_)
        {
            return true;
        }

        struct timeval timeout = {config_.connection_timeout, 0};
        context_ = redisConnectWithTimeout(config_.host.c_str(), config_.port, timeout);

        if (!context_ || context_->err)
        {
            if (context_)
            {
                redisFree(context_);
                context_ = nullptr;
            }
            return false;
        }

        if (!config_.password.empty())
        {
            auto reply = executeCommand("AUTH %s", config_.password.c_str());
            if (!reply.isOk() || reply.isNil())
            {
                disconnect();
                return false;
            }
        }

        if (config_.database != 0)
        {
            auto reply = executeCommand("SELECT %d", config_.database);
            if (!reply.isOk() || reply.isNil())
            {
                disconnect();
                return false;
            }
        }

        return true;
    }

    void RedisRepository::disconnect()
    {
        if (context_)
        {
            redisFree(context_);
            context_ = nullptr;
        }
    }

    bool RedisRepository::isConnected() const
    {
        return context_ != nullptr && !context_->err;
    }

    std::optional<std::string> RedisRepository::get(const std::string& key)
    {
        auto reply = executeCommand("GET %s", key.c_str());
        if (reply.isNil())
        {
            return std::nullopt;
        }
        if (reply.isString())
        {
            return reply.asString();
        }
        return std::nullopt;
    }

    bool RedisRepository::set(const std::string& key, const std::string& value, int expire_seconds)
    {
        auto reply = (expire_seconds > 0)
                         ? executeCommand("SETEX %s %d %b", key.c_str(), expire_seconds, value.data(), value.size())
                         : executeCommand("SET %s %b", key.c_str(), value.data(), value.size());
        return reply.isOk() && (reply.isString() || reply.isOk());
    }

    bool RedisRepository::del(const std::string& key)
    {
        auto reply = executeCommand("DEL %s", key.c_str());
        return reply.isOk() && reply.asInteger() > 0;
    }

    bool RedisRepository::expire(const std::string& key, int seconds)
    {
        auto reply = executeCommand("EXPIRE %s %d", key.c_str(), seconds);
        return reply.isOk() && reply.asInteger() == 1;
    }

    int64_t RedisRepository::ttl(const std::string& key)
    {
        auto reply = executeCommand("TTL %s", key.c_str());
        return reply.isInteger() ? reply.asInteger() : -1;
    }

    bool RedisRepository::exists(const std::string& key)
    {
        auto reply = executeCommand("EXISTS %s", key.c_str());
        return reply.isOk() && reply.asInteger() > 0;
    }

    std::vector<std::optional<std::string>> RedisRepository::mget(const std::vector<std::string>& keys)
    {
        std::vector<std::optional<std::string>> results;
        if (keys.empty())
        {
            return results;
        }

        std::string cmd = "MGET";
        for (const auto& key : keys)
        {
            cmd += " " + key;
        }

        auto reply = executeCommand(cmd.c_str());
        if (reply.isArray())
        {
            for (size_t i = 0; i < reply.arraySize(); ++i)
            {
                auto element = reply.elementAt(i);
                if (element.isNil())
                {
                    results.push_back(std::nullopt);
                }
                else if (element.isString())
                {
                    results.push_back(element.asString());
                }
            }
        }

        return results;
    }

    bool RedisRepository::mset(const std::vector<std::pair<std::string, std::string>>& kv_pairs)
    {
        if (kv_pairs.empty())
        {
            return true;
        }

        std::string cmd = "MSET";
        for (const auto& [key, value] : kv_pairs)
        {
            cmd += " " + key + " " + value;
        }

        auto reply = executeCommand(cmd.c_str());
        return reply.isOk();
    }

    int64_t RedisRepository::incr(const std::string& key)
    {
        auto reply = executeCommand("INCR %s", key.c_str());
        return reply.isInteger() ? reply.asInteger() : 0;
    }

    int64_t RedisRepository::incrBy(const std::string& key, int64_t increment)
    {
        auto reply = executeCommand("INCRBY %s %lld", key.c_str(), increment);
        return reply.isInteger() ? reply.asInteger() : 0;
    }

    int64_t RedisRepository::decr(const std::string& key)
    {
        auto reply = executeCommand("DECR %s", key.c_str());
        return reply.isInteger() ? reply.asInteger() : 0;
    }

    bool RedisRepository::hset(const std::string& name, const std::string& key, const std::string& value)
    {
        auto reply = executeCommand("HSET %s %s %b", name.c_str(), key.c_str(), value.data(), value.size());
        return reply.isOk();
    }

    std::optional<std::string> RedisRepository::hget(const std::string& name, const std::string& key)
    {
        auto reply = executeCommand("HGET %s %s", name.c_str(), key.c_str());
        if (reply.isNil())
        {
            return std::nullopt;
        }
        if (reply.isString())
        {
            return reply.asString();
        }
        return std::nullopt;
    }

    bool RedisRepository::hdel(const std::string& name, const std::string& key)
    {
        auto reply = executeCommand("HDEL %s %s", name.c_str(), key.c_str());
        return reply.isOk() && reply.asInteger() > 0;
    }

    std::vector<std::string> RedisRepository::hkeys(const std::string& name)
    {
        auto reply = executeCommand("HKEYS %s", name.c_str());
        return reply.asArray();
    }

    std::vector<std::string> RedisRepository::hvals(const std::string& name)
    {
        auto reply = executeCommand("HVALS %s", name.c_str());
        return reply.asArray();
    }

    int64_t RedisRepository::lpush(const std::string& key, const std::string& value)
    {
        auto reply = executeCommand("LPUSH %s %b", key.c_str(), value.data(), value.size());
        return reply.isInteger() ? reply.asInteger() : 0;
    }

    int64_t RedisRepository::rpush(const std::string& key, const std::string& value)
    {
        auto reply = executeCommand("RPUSH %s %b", key.c_str(), value.data(), value.size());
        return reply.isInteger() ? reply.asInteger() : 0;
    }

    std::optional<std::string> RedisRepository::lpop(const std::string& key)
    {
        auto reply = executeCommand("LPOP %s", key.c_str());
        if (reply.isNil())
        {
            return std::nullopt;
        }
        if (reply.isString())
        {
            return reply.asString();
        }
        return std::nullopt;
    }

    std::optional<std::string> RedisRepository::rpop(const std::string& key)
    {
        auto reply = executeCommand("RPOP %s", key.c_str());
        if (reply.isNil())
        {
            return std::nullopt;
        }
        if (reply.isString())
        {
            return reply.asString();
        }
        return std::nullopt;
    }

    std::vector<std::string> RedisRepository::lrange(const std::string& key, int64_t start, int64_t stop)
    {
        auto reply = executeCommand("LRANGE %s %lld %lld", key.c_str(), start, stop);
        return reply.asArray();
    }

    RedisReply RedisRepository::executeCommand(const char* format, ...)
    {
        if (!context_)
        {
            return RedisReply(nullptr);
        }

        va_list ap;
        va_start(ap, format);
        redisReply* reply = static_cast<redisReply*>(redisvCommand(context_, format, ap));
        va_end(ap);

        return RedisReply(reply);
    }

    RedisReply RedisRepository::executeCommandArgs(int argc, const char** argv, const size_t* argvlen)
    {
        if (!context_)
        {
            return RedisReply(nullptr);
        }

        redisReply* reply = static_cast<redisReply*>(redisCommandArgv(context_, argc, argv, argvlen));
        return RedisReply(reply);
    }

}  // namespace soilwatercm::repository
