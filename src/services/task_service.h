#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../dao/task_dao.h"
#include "../models/task.h"

namespace soilwatercm::services
{

    /**
     * @brief 异步任务管理服务
     *
     * 管理任务生命周期: 建任务、查任务、更新状态/进度,
     * 以及重试、恢复、强杀、取消、删除等状态流转操作。
     */
    class TaskService
    {
    public:
        /**
         * @brief 构造, 注入任务 DAO
         * @param taskDao 任务 DAO
         */
        explicit TaskService(std::shared_ptr<dao::TaskDao> taskDao);

        ~TaskService() = default;

        /**
         * @brief 创建任务记录 (初始状态 Pending)
         * @param type 任务类型
         * @param projectId 项目 id; 0 表示不归属项目
         * @param periodId 期次 id; 0 表示不归属期次
         * @param params 任务参数 (JSON 字符串)
         * @return 含新 id 的任务记录
         */
        models::Task createTask(models::TaskType type, int projectId = 0, int periodId = 0,
                                const std::string& params = "");

        /**
         * @brief 按 id 查任务
         * @param id 任务主键
         * @return 任务; 不存在返回 nullopt
         */
        std::optional<models::Task> getTaskById(int id);

        /**
         * @brief 按项目查任务列表
         * @param projectId 项目 id
         * @return 任务列表
         */
        std::vector<models::Task> getTasksByProject(int projectId);

        /**
         * @brief 按状态查任务列表
         * @param status 任务状态
         * @return 任务列表
         */
        std::vector<models::Task> getTasksByStatus(models::TaskStatus status);

        /**
         * @brief 更新任务状态
         * @param id 任务主键
         * @param status 新状态; 转 Running 记开始时间, 转 Completed/Failed 记完成时间
         * @return 更新成功返回 true
         */
        bool updateTaskStatus(int id, models::TaskStatus status);

        /**
         * @brief 更新进度百分比与说明
         * @param id 任务主键
         * @param progress 进度百分比
         * @param message 进度说明
         * @return 更新成功返回 true
         */
        bool updateTaskProgress(int id, int progress, const std::string& message = "");

        /**
         * @brief 重试任务
         * @param id 任务主键
         * @return 仅失败任务可重, 成功则回到 Pending 并累加重试次数; 否则 false
         */
        bool retryTask(int id);

        /**
         * @brief 恢复任务
         * @param id 任务主键
         * @return 非完成任务回到 Pending 并清空错误信息; 否则 false
         */
        bool resumeTask(int id);

        /**
         * @brief 强杀任务
         * @param id 任务主键
         * @return 立即落 Cancelled 终态并写错误说明 (不做进程终止)
         */
        bool forceKillTask(int id);

        /**
         * @brief 取消任务
         * @param id 任务主键
         * @return 非完成任务置 Cancelled 并记完成时间; 否则 false
         */
        bool cancelTask(int id);

        /**
         * @brief 删除任务记录
         * @param id 任务主键
         * @return 删除成功返回 true
         */
        bool deleteTask(int id);

        /**
         * @brief 活跃任务数
         * @return Running + Pending 的任务数
         */
        int getActiveTaskCount();

        /**
         * @brief 最近任务列表
         * @param limit 条数上限, 默认 10
         * @return 按创建时间倒序的任务列表
         */
        std::vector<models::Task> getRecentTasks(int limit = 10);

    private:
        std::shared_ptr<dao::TaskDao> taskDao_;
        std::atomic<int> taskIdCounter_{0};
    };

}  // namespace soilwatercm::services
