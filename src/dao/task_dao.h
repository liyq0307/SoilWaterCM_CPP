#pragma once
#include "../models/task.h"
#include "dao_base.h"
namespace soilwatercm::dao
{
    /**
     * @brief 异步任务数据访问对象
     *
     * 对应 tasks 表: 异步任务 (类型、状态、进度、参数、结果路径、重试次数)。
     */
    class TaskDao : public DaoBase<models::Task>
    {
    public:
        /**
         * @brief 构造, 注入数据库仓库
         * @param repo PostgreSQL 访问仓库
         */
        explicit TaskDao(std::shared_ptr<repository::PostgresRepository> repo);

        /**
         * @brief 按 id 查任务
         * @param id 任务主键
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::Task> findById(int id) override;

        /**
         * @brief 按项目查任务
         * @param projectId 项目 id
         * @return 匹配记录, 按提交时间倒序
         */
        virtual std::vector<models::Task> findByProjectId(int projectId);

        /**
         * @brief 按状态查任务 (如轮询待执行任务)
         * @param status 任务状态
         * @return 匹配记录列表
         */
        virtual std::vector<models::Task> findByStatus(models::TaskStatus status);

        /**
         * @brief 分页列出所有任务
         * @param page 页码, 从 1 开始
         * @param pageSize 每页条数
         * @param orderBy 排序列名
         * @param ascending 是否升序
         * @return 当前页记录列表
         */
        std::vector<models::Task> list(int page = 1, int pageSize = 20, const std::string& orderBy = "id",
                                       bool ascending = true) override;

        /**
         * @brief 新增任务
         * @param t 待插入记录
         * @return 新记录 id; 失败返回 -1
         */
        int insert(const models::Task& t) override;

        /**
         * @brief 按 id 更新任务状态和进度
         * @param t 记录 (须含 id)
         * @return 更新成功返回 true
         */
        bool update(const models::Task& t) override;

        /**
         * @brief 按 id 删除任务
         * @param id 任务主键
         * @return 删除成功返回 true
         */
        bool remove(int id) override;

        /**
         * @brief 任务总数
         * @return 表内记录数
         */
        int count() override;

        /**
         * @brief 最近提交的任务列表
         * @param limit 最多返回条数
         * @return 任务列表, 按提交时间倒序
         */
        virtual std::vector<models::Task> listRecent(int limit = 10);

        /**
         * @brief 更新状态和进度
         *
         * 转运行写 started_at, 转完成/失败写 completed_at。
         * @param id 任务主键
         * @param status 新状态
         * @param progress 新进度
         * @return 更新成功返回 true
         */
        virtual bool updateStatus(int id, models::TaskStatus status, double progress);

        /**
         * @brief 更新状态、进度和错误信息
         *
         * 转完成/失败写 completed_at; 转排队清空 started_at/completed_at。
         * @param id 任务主键
         * @param status 新状态
         * @param progress 新进度
         * @param errorMessage 错误信息; 为空写 NULL
         * @return 更新成功返回 true
         */
        virtual bool updateStatusAndMessage(int id, models::TaskStatus status, double progress,
                                             const std::string& errorMessage);

        /**
         * @brief 记录任务结果文件路径
         * @param id 任务主键
         * @param resultPath 结果文件路径
         * @return 更新成功返回 true
         */
        virtual bool updateResult(int id, const std::string& resultPath);

        /**
         * @brief 重试次数加一
         * @param id 任务主键
         * @return 更新成功返回 true
         */
        virtual bool incrementRetryCount(int id);

    private:
        /// @brief 行数据 -> 实体
        models::Task mapRowToTask(const std::map<std::string, std::string>& row);
    };
}  // namespace soilwatercm::dao
