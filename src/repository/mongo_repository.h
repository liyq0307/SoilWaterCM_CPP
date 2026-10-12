#pragma once

#include <memory>
#include <string>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>
#include <mongocxx/uri.hpp>

namespace soilwatercm::repository
{

    /// MongoDB 连接与池参数
    struct MongoConfig
    {
        std::string host = "localhost";
        int port = 27017;
        std::string database = "soilwatercm_meta";
        std::string user = "";
        std::string password = "";
        std::string auth_source = "admin";
        size_t min_connections = 3;
        size_t max_connections = 10;
        int connection_timeout = 30;
        int max_idle_time = 60000;
    };

    /**
     * @brief 单条 MongoDB 连接的包装
     *
     * 绑定默认库名。
     */
    class MongoConnection
    {
    public:
        /**
         * @brief 构造, 接管 mongocxx 客户端
         * @param client 底层客户端
         * @param database 默认库名
         */
        explicit MongoConnection(mongocxx::client&& client, const std::string& database);
        ~MongoConnection() = default;

        /// @brief 底层客户端引用
        mongocxx::client& getClient();

        /// @brief 默认库
        mongocxx::database getDatabase();

        /// @brief 默认库下的集合
        mongocxx::collection getCollection(const std::string& name);

        /// @brief ping 命令探活
        bool isValid() const;

        MongoConnection(const MongoConnection&) = delete;
        MongoConnection& operator=(const MongoConnection&) = delete;
        MongoConnection(MongoConnection&&) = default;
        MongoConnection& operator=(MongoConnection&&) = default;

    private:
        mongocxx::client client_;
        std::string database_;
    };

    /**
     * @brief MongoDB 连接池
     *
     * 底层直接用 mongocxx::pool, 本类只做 URI 拼装和包装。
     */
    class MongoConnectionPool
    {
    public:
        /**
         * @brief 构造, 拼 URI 并建 mongocxx 池
         * @param config 连接参数
         */
        explicit MongoConnectionPool(const MongoConfig& config);
        ~MongoConnectionPool() = default;

        /**
         * @brief 获取一条连接
         * @return 连接包装; 获取失败返回 nullptr
         */
        std::unique_ptr<MongoConnection> acquire();

        /// @brief mongocxx 池在包装对象析构时自动回收连接, 这里无需处理
        void release(std::unique_ptr<MongoConnection> conn);

        /// @brief 池容量上限 (当前直接返回 max_connections, 非实时计数)
        size_t size() const;

        /// @brief 可用连接数 (同上, 返回 max_connections)
        size_t available() const;

        MongoConnectionPool(const MongoConnectionPool&) = delete;
        MongoConnectionPool& operator=(const MongoConnectionPool&) = delete;
        MongoConnectionPool(MongoConnectionPool&&) = delete;
        MongoConnectionPool& operator=(MongoConnectionPool&&) = delete;

    private:
        std::string buildUri(const MongoConfig& config) const;

        MongoConfig config_;
        std::unique_ptr<mongocxx::instance> instance_;
        std::unique_ptr<mongocxx::pool> pool_;
        std::string database_;
    };

    /**
     * @brief RAII 守卫
     *
     * 离开作用域自动归还连接到池。
     */
    class MongoConnectionGuard
    {
    public:
        /**
         * @brief 构造, 接管连接
         * @param conn 借出的连接
         * @param pool 归还目标池
         */
        MongoConnectionGuard(std::unique_ptr<MongoConnection> conn, MongoConnectionPool& pool);
        ~MongoConnectionGuard();

        /// @brief 底层客户端引用
        mongocxx::client& getClient();

        /// @brief 默认库
        mongocxx::database getDatabase();

        /// @brief 默认库下的集合
        mongocxx::collection getCollection(const std::string& name);

        MongoConnectionGuard(const MongoConnectionGuard&) = delete;
        MongoConnectionGuard& operator=(const MongoConnectionGuard&) = delete;
        MongoConnectionGuard(MongoConnectionGuard&&) = default;
        MongoConnectionGuard& operator=(MongoConnectionGuard&&) = default;

    private:
        std::unique_ptr<MongoConnection> conn_;
        MongoConnectionPool* pool_;
    };

}  // namespace soilwatercm::repository
