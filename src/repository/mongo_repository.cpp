#include "mongo_repository.h"

#include <sstream>
#include <stdexcept>

namespace soilwatercm::repository
{

    MongoConnection::MongoConnection(mongocxx::client&& client, const std::string& database)
        : client_(std::move(client)), database_(database)
    {
    }

    mongocxx::client& MongoConnection::getClient()
    {
        return client_;
    }

    mongocxx::database MongoConnection::getDatabase()
    {
        return client_[database_];
    }

    mongocxx::collection MongoConnection::getCollection(const std::string& name)
    {
        return client_[database_][name];
    }

    bool MongoConnection::isValid() const
    {
        try
        {
            auto result = client_[database_].run_command(
                bsoncxx::builder::basic::make_document(bsoncxx::builder::basic::kvp("ping", 1)));
            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    MongoConnectionPool::MongoConnectionPool(const MongoConfig& config) : config_(config), database_(config.database)
    {
        instance_ = std::make_unique<mongocxx::instance>();

        std::string uri = buildUri(config);
        mongocxx::uri mongocxx_uri(uri);

        pool_ = std::make_unique<mongocxx::pool>(mongocxx_uri);
    }

    std::unique_ptr<MongoConnection> MongoConnectionPool::acquire()
    {
        try
        {
            auto client = pool_->acquire();
            return std::make_unique<MongoConnection>(std::move(*client), database_);
        }
        catch (const std::exception&)
        {
            return nullptr;
        }
    }

    void MongoConnectionPool::release(std::unique_ptr<MongoConnection> conn) {}

    size_t MongoConnectionPool::size() const
    {
        return config_.max_connections;
    }

    size_t MongoConnectionPool::available() const
    {
        return config_.max_connections;
    }

    std::string MongoConnectionPool::buildUri(const MongoConfig& config) const
    {
        std::ostringstream oss;

        oss << "mongodb://";

        if (!config.user.empty() && !config.password.empty())
        {
            oss << config.user << ":" << config.password << "@";
        }

        oss << config.host << ":" << config.port << "/" << config.database;

        if (!config.user.empty())
        {
            oss << "?authSource=" << config.auth_source;
        }

        return oss.str();
    }

    MongoConnectionGuard::MongoConnectionGuard(std::unique_ptr<MongoConnection> conn, MongoConnectionPool& pool)
        : conn_(std::move(conn)), pool_(&pool)
    {
    }

    MongoConnectionGuard::~MongoConnectionGuard()
    {
        if (conn_ && pool_)
        {
            pool_->release(std::move(conn_));
        }
    }

    mongocxx::client& MongoConnectionGuard::getClient()
    {
        return conn_->getClient();
    }

    mongocxx::database MongoConnectionGuard::getDatabase()
    {
        return conn_->getDatabase();
    }

    mongocxx::collection MongoConnectionGuard::getCollection(const std::string& name)
    {
        return conn_->getCollection(name);
    }

}  // namespace soilwatercm::repository
