#include "postgres_repository.h"

#include <stdexcept>

namespace soilwatercm::repository
{

    PostgresConnection::PostgresConnection(const PostgresConfig& config)
        : config_(config), last_used_(std::chrono::steady_clock::now())
    {
        std::string conn_str = "host=" + config.host + " port=" + std::to_string(config.port) +
                               " dbname=" + config.database + " user=" + config.user + " password=" + config.password;

        connection_ = std::make_unique<pqxx::connection>(conn_str);
    }

    PostgresConnection::~PostgresConnection() = default;

    PostgresConnection::PostgresConnection(PostgresConnection&& other) noexcept
        : connection_(std::move(other.connection_)), config_(std::move(other.config_)), last_used_(other.last_used_)
    {
    }

    PostgresConnection& PostgresConnection::operator=(PostgresConnection&& other) noexcept
    {
        if (this != &other)
        {
            connection_ = std::move(other.connection_);
            config_ = std::move(other.config_);
            last_used_ = other.last_used_;
        }
        return *this;
    }

    pqxx::connection& PostgresConnection::get()
    {
        if (!connection_)
        {
            throw std::runtime_error("Connection is null");
        }
        return *connection_;
    }

    bool PostgresConnection::isValid() const
    {
        return connection_ && connection_->is_open();
    }

    bool PostgresConnection::ping()
    {
        try
        {
            if (connection_ && connection_->is_open())
            {
                pqxx::work txn(*connection_);
                txn.exec("SELECT 1");
                txn.commit();
                return true;
            }
        }
        catch (const std::exception&)
        {
            return false;
        }
        return false;
    }

    std::chrono::steady_clock::time_point PostgresConnection::getLastUsed() const
    {
        return last_used_;
    }

    void PostgresConnection::updateLastUsed()
    {
        last_used_ = std::chrono::steady_clock::now();
    }

    PostgresConnectionPool::PostgresConnectionPool(const PostgresConfig& config) : config_(config)
    {
        warmup();
    }

    PostgresConnectionPool::~PostgresConnectionPool()
    {
        shutdown_ = true;
        condition_.notify_all();

        std::lock_guard<std::mutex> lock(mutex_);
        while (!pool_.empty())
        {
            pool_.pop();
        }
    }

    std::unique_ptr<PostgresConnection> PostgresConnectionPool::acquire()
    {
        std::unique_lock<std::mutex> lock(mutex_);

        condition_.wait(lock,
                        [this]() { return shutdown_ || !pool_.empty() || in_use_count_ < config_.max_connections; });

        if (shutdown_)
        {
            return nullptr;
        }

        if (!pool_.empty())
        {
            auto conn = std::move(pool_.front());
            pool_.pop();

            if (conn && conn->isValid() && conn->ping())
            {
                conn->updateLastUsed();
                ++in_use_count_;
                return conn;
            }
        }

        if (in_use_count_ < config_.max_connections)
        {
            auto conn = createConnection();
            if (conn)
            {
                ++in_use_count_;
                return conn;
            }
        }

        return nullptr;
    }

    void PostgresConnectionPool::release(std::unique_ptr<PostgresConnection> conn)
    {
        if (!conn)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        if (shutdown_ || !conn->isValid())
        {
            --in_use_count_;
            return;
        }

        conn->updateLastUsed();
        pool_.push(std::move(conn));
        --in_use_count_;
        condition_.notify_one();
    }

    size_t PostgresConnectionPool::size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_.size() + in_use_count_;
    }

    size_t PostgresConnectionPool::available() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_.size();
    }

    size_t PostgresConnectionPool::inUse() const
    {
        return in_use_count_;
    }

    void PostgresConnectionPool::warmup()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (size_t i = 0; i < config_.min_connections; ++i)
        {
            auto conn = createConnection();
            if (conn)
            {
                pool_.push(std::move(conn));
            }
        }
    }

    void PostgresConnectionPool::cleanup()
    {
        cleanupIdleConnections();
    }

    std::unique_ptr<PostgresConnection> PostgresConnectionPool::createConnection()
    {
        try
        {
            return std::make_unique<PostgresConnection>(config_);
        }
        catch (const std::exception&)
        {
            return nullptr;
        }
    }

    void PostgresConnectionPool::cleanupIdleConnections()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto now = std::chrono::steady_clock::now();
        std::queue<std::unique_ptr<PostgresConnection>> active_connections;

        while (!pool_.empty())
        {
            auto conn = std::move(pool_.front());
            pool_.pop();

            if (conn)
            {
                auto idle_time = std::chrono::duration_cast<std::chrono::seconds>(now - conn->getLastUsed()).count();

                if (idle_time < static_cast<decltype(idle_time)>(config_.idle_timeout) && conn->isValid())
                {
                    active_connections.push(std::move(conn));
                }
            }
        }

        pool_ = std::move(active_connections);
    }

    PostgresConnectionGuard::PostgresConnectionGuard(std::unique_ptr<PostgresConnection> conn,
                                                     PostgresConnectionPool& pool)
        : conn_(std::move(conn)), pool_(&pool)
    {
    }

    PostgresConnectionGuard::~PostgresConnectionGuard()
    {
        if (conn_ && pool_)
        {
            pool_->release(std::move(conn_));
        }
    }

    pqxx::connection& PostgresConnectionGuard::get()
    {
        return conn_->get();
    }

    pqxx::connection* PostgresConnectionGuard::operator->()
    {
        return &conn_->get();
    }

    PostgresRepository::PostgresRepository(std::shared_ptr<PostgresConnectionPool> pool) : pool_(std::move(pool)) {}

    std::vector<std::map<std::string, std::string>> PostgresRepository::executeQuery(const std::string& query)
    {
        std::vector<std::map<std::string, std::string>> results;
        auto conn = pool_->acquire();
        if (!conn)
            return results;
        PostgresConnectionGuard guard(std::move(conn), *pool_);
        try
        {
            pqxx::work txn(guard.get());
            auto result = txn.exec(query);
            txn.commit();
            for (const auto& row : result)
            {
                std::map<std::string, std::string> rowMap;
                for (pqxx::row::size_type i = 0; i < row.size(); ++i)
                {
                    rowMap[result.column_name(i)] = row[i].as<std::string>();
                }
                results.push_back(std::move(rowMap));
            }
        }
        catch (const std::exception&)
        {
        }
        return results;
    }

    bool PostgresRepository::executeUpdate(const std::string& query)
    {
        auto conn = pool_->acquire();
        if (!conn)
            return false;
        PostgresConnectionGuard guard(std::move(conn), *pool_);
        try
        {
            pqxx::work txn(guard.get());
            auto result = txn.exec(query);
            txn.commit();
            return result.affected_rows() > 0;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

}  // namespace soilwatercm::repository
