#pragma once

#include <drogon/HttpController.h>

#include "../services/task_service.h"

namespace soilwatercm::controllers
{

    /**
     * @brief 异步任务管理 HTTP 控制器
     *
     * 提供任务查询与重试/恢复/取消/终止, 路由前缀 /api/v1/tasks。
     */
    class TaskController : public drogon::HttpController<TaskController, false>
    {
    public:
        TaskController() = default;

        /**
         * @brief 构造, 注入任务服务
         * @param taskService 异步任务服务
         */
        explicit TaskController(std::shared_ptr<services::TaskService> taskService);

        METHOD_LIST_BEGIN
        ADD_METHOD_TO(TaskController::listTasks, "/api/v1/tasks", drogon::Get);
        ADD_METHOD_TO(TaskController::getTask, "/api/v1/tasks/{id}", drogon::Get);
        ADD_METHOD_TO(TaskController::retryTask, "/api/v1/tasks/{id}/retry", drogon::Post);
        ADD_METHOD_TO(TaskController::resumeTask, "/api/v1/tasks/{id}/resume", drogon::Post);
        ADD_METHOD_TO(TaskController::forceKillTask, "/api/v1/tasks/{id}/force-kill", drogon::Post);
        ADD_METHOD_TO(TaskController::cancelTask, "/api/v1/tasks/{id}/cancel", drogon::Post);
        ADD_METHOD_TO(TaskController::deleteTask, "/api/v1/tasks/{id}", drogon::Delete);
        METHOD_LIST_END

        /**
         * @brief GET /api/v1/tasks?status=|project_id= 列出任务
         * @return 通过 callback 返回任务列表; 按状态或项目过滤, 缺省返回最近 100 条
         */
        void listTasks(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/tasks/{id} 查询任务详情
         * @param id 任务 id
         * @return 通过 callback 返回任务记录 (参数/结果路径/错误信息)
         */
        void getTask(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     int id);

        /**
         * @brief POST /api/v1/tasks/{id}/retry 重试失败任务
         * @param id 任务 id
         * @return 通过 callback 返回重试后的任务
         */
        void retryTask(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief POST /api/v1/tasks/{id}/resume 恢复中断任务
         * @param id 任务 id
         * @return 通过 callback 返回恢复后的任务; 状态置回 pending
         */
        void resumeTask(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief POST /api/v1/tasks/{id}/force-kill 强制终止运行中任务
         * @param id 任务 id
         * @return 通过 callback 返回终止后的任务; 仅归档状态, 不杀进程
         */
        void forceKillTask(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief POST /api/v1/tasks/{id}/cancel 取消任务
         * @param id 任务 id
         * @return 通过 callback 返回取消后的任务
         */
        void cancelTask(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief DELETE /api/v1/tasks/{id} 删除任务记录
         * @param id 任务 id
         * @return 通过 callback 返回 204
         */
        void deleteTask(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

    private:
        std::shared_ptr<services::TaskService> taskService_;
    };

}  // namespace soilwatercm::controllers
