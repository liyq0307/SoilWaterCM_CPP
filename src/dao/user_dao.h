#pragma once

#include "../models/user.h"
#include "dao_base.h"

namespace soilwatercm::dao
{

    /**
     * @brief 系统用户数据访问对象
     *
     * 对应 users 表: 系统用户 (账号、密码哈希、姓名、联系方式、角色、启用状态)。
     */
    class UserDao : public DaoBase<models::User>
    {
    public:
        /**
         * @brief 构造, 注入数据库仓库
         * @param repo PostgreSQL 访问仓库
         */
        explicit UserDao(std::shared_ptr<repository::PostgresRepository> repo);

        /// @brief 虚析构, 支持多态删除
        ~UserDao() override = default;

        /**
         * @brief 按 id 查用户
         * @param id 用户主键
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::User> findById(int id) override;

        /**
         * @brief 按用户名查用户 (登录用)
         * @param username 用户名
         * @return 记录; 不存在返回 nullopt
         */
        virtual std::optional<models::User> findByUsername(const std::string& username);

        /**
         * @brief 分页列出所有用户
         * @param page 页码, 从 1 开始
         * @param pageSize 每页条数
         * @param orderBy 排序列名
         * @param ascending 是否升序
         * @return 当前页记录列表
         */
        std::vector<models::User> list(int page = 1, int pageSize = 20, const std::string& orderBy = "id",
                                       bool ascending = true) override;

        /**
         * @brief 新增用户
         * @param user 待插入记录
         * @return 新记录 id; 失败返回 -1
         */
        int insert(const models::User& user) override;

        /**
         * @brief 按 id 更新用户资料、角色、启用状态 (不含密码)
         * @param user 记录 (须含 id)
         * @return 更新成功返回 true
         */
        bool update(const models::User& user) override;

        /**
         * @brief 按 id 删除用户
         * @param id 用户主键
         * @return 删除成功返回 true
         */
        bool remove(int id) override;

        /**
         * @brief 用户总数
         * @return 表内记录数
         */
        int count() override;

        /**
         * @brief 把 last_login 刷成当前时间
         * @param userId 用户 id
         * @return 更新成功返回 true
         */
        virtual bool updateLastLogin(int userId);

        /**
         * @brief 用户名是否已被占用
         * @param username 用户名
         * @return 已占用返回 true
         */
        virtual bool existsByUsername(const std::string& username);

        /**
         * @brief 按角色查用户
         * @param role 用户角色
         * @return 匹配记录列表
         */
        std::vector<models::User> findByRole(models::UserRole role);

    private:
        /// @brief 行数据 -> 实体
        models::User mapRowToUser(const std::map<std::string, std::string>& row);
    };

}  // namespace soilwatercm::dao
