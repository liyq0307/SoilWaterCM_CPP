#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include <pqxx/pqxx>

namespace soilwatercm::repository
{

    /// PostgreSQL 连接与池参数
    struct PostgresConfig
    {
        std::string host = "localhost";
        int port = 5432;
        std::string database = "soilwatercm";
        std::string user = "postgres";
        std::string password = "";
        size_t min_connections = 5;
        size_t max_connections = 20;
        int connection_timeout = 30;
        int idle_timeout = 600;
    };

    /**
     * @brief 单条 pqxx 连接的包装
     *
     * 记录最后使用时间, 供连接池回收空闲连接。
     */
    class PostgresConnection
    {
    public:
        /**
         * @brief 构造并建立连接
         * @param config 连接参数
         */
        explicit PostgresConnection(const PostgresConfig& config);
        ~PostgresConnection();

        /// @brief 底层 pqxx 连接引用
        pqxx::connection& get();

        /// @brief 连接是否仍然打开
        bool isValid() const;

        /// @brief 发一条轻量查询探活
        bool ping();

        /// @brief 最后使用时间 (池回收依据)
        std::chrono::steady_clock::time_point getLastUsed() const;

        /// @brief 刷新最后使用时间
        void updateLastUsed();

        PostgresConnection(const PostgresConnection&) = delete;
        PostgresConnection& operator=(const PostgresConnection&) = delete;
        PostgresConnection(PostgresConnection&&) noexcept;
        PostgresConnection& operator=(PostgresConnection&&) noexcept;

    private:
        std::unique_ptr<pqxx::connection> connection_;
        PostgresConfig config_;
        std::chrono::steady_clock::time_point last_used_;
    };

    /**
     * @brief PostgreSQL 连接池
     *
     * 按需新建, 上限 max_connections, 空闲超时的连接定期回收。
     */
    class PostgresConnectionPool
    {
    public:
        /**
         * @brief 构造, 暂不建连
         * @param config 池参数
         */
        explicit PostgresConnectionPool(const PostgresConfig& config);
        ~PostgresConnectionPool();

        /**
         * @brief 获取一条可用连接
         *
         * 池空且未达上限则新建, 否则阻塞等待归还。
         * @return 连接句柄; 池已关闭返回 nullptr
         */
        std::unique_ptr<PostgresConnection> acquire();

        /// @brief 归还连接到池中
        void release(std::unique_ptr<PostgresConnection> conn);

        /// @brief 连接总数 (空闲 + 借出)
        size_t size() const;

        /// @brief 池中空闲连接数
        size_t available() const;

        /// @brief 已借出的连接数
        size_t inUse() const;

        /// @brief 预先建满 min_connections 条, 避免首个请求现连
        void warmup();

        /// @brief 清理空闲超时的连接
        void cleanup();

        PostgresConnectionPool(const PostgresConnectionPool&) = delete;
        PostgresConnectionPool& operator=(const PostgresConnectionPool&) = delete;
        PostgresConnectionPool(PostgresConnectionPool&&) = delete;
        PostgresConnectionPool& operator=(PostgresConnectionPool&&) = delete;

    private:
        std::unique_ptr<PostgresConnection> createConnection();
        void cleanupIdleConnections();

        PostgresConfig config_;
        std::queue<std::unique_ptr<PostgresConnection>> pool_;
        mutable std::mutex mutex_;
        std::condition_variable condition_;
        std::atomic<size_t> in_use_count_{0};
        std::atomic<bool> shutdown_{false};
    };

    /**
     * @brief RAII 守卫
     *
     * 离开作用域自动把连接归还连接池。
     */
    class PostgresConnectionGuard
    {
    public:
        /**
         * @brief 构造, 接管连接
         * @param conn 借出的连接
         * @param pool 归还目标池
         */
        PostgresConnectionGuard(std::unique_ptr<PostgresConnection> conn, PostgresConnectionPool& pool);
        ~PostgresConnectionGuard();

        /// @brief 底层 pqxx 连接引用
        pqxx::connection& get();

        /// @brief 指针访问底层连接
        pqxx::connection* operator->();

        PostgresConnectionGuard(const PostgresConnectionGuard&) = delete;
        PostgresConnectionGuard& operator=(const PostgresConnectionGuard&) = delete;
        PostgresConnectionGuard(PostgresConnectionGuard&&) = default;
        PostgresConnectionGuard& operator=(PostgresConnectionGuard&&) = default;

    private:
        std::unique_ptr<PostgresConnection> conn_;
        PostgresConnectionPool* pool_;
    };

    /**
     * @brief PostgreSQL 数据访问入口
     *
     * 借连接执行 SQL, 结果转成列名到字符串的 map 列表。
     */
    class PostgresRepository
    {
    public:
        /**
         * @brief 构造, 注入连接池
         * @param pool PostgreSQL 连接池
         */
        explicit PostgresRepository(std::shared_ptr<PostgresConnectionPool> pool);
        ~PostgresRepository() = default;

        /**
         * @brief 执行查询并取回全部行
         * @param query SQL 语句
         * @return 行列表 (列名 -> 文本值); 取不到连接或执行异常返回空
         */
        std::vector<std::map<std::string, std::string>> executeQuery(const std::string& query);

        /**
         * @brief 执行增删改
         * @param query SQL 语句
         * @return 影响行数大于 0 返回 true
         */
        bool executeUpdate(const std::string& query);

        PostgresRepository(const PostgresRepository&) = delete;
        PostgresRepository& operator=(const PostgresRepository&) = delete;
        PostgresRepository(PostgresRepository&&) = default;
        PostgresRepository& operator=(PostgresRepository&&) = default;

    private:
        std::shared_ptr<PostgresConnectionPool> pool_;
    };

}  // namespace soilwatercm::repository
