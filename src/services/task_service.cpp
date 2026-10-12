#include "task_service.h"

#include "../utils/logger.h"

namespace soilwatercm::services
{

    TaskService::TaskService(std::shared_ptr<dao::TaskDao> taskDao) : taskDao_(std::move(taskDao)) {}

    models::Task TaskService::createTask(models::TaskType type, int projectId, int periodId, const std::string& params)
    {
        models::Task task;
        task.type = type;
        task.status = models::TaskStatus::Pending;
        task.params = params;
        task.project_id = projectId;
        task.period_id = periodId;
        task.retry_count = 0;
        task.submitted_at = std::chrono::system_clock::now();

        int taskId = taskDao_->insert(task);
        task.id = taskId;

        utils::Logger::info("Task created: id={}, type={}", taskId, static_cast<int>(type));

        return task;
    }

    std::optional<models::Task> TaskService::getTaskById(int id)
    {
        return taskDao_->findById(id);
    }

    std::vector<models::Task> TaskService::getTasksByProject(int projectId)
    {
        return taskDao_->findByProjectId(projectId);
    }

    std::vector<models::Task> TaskService::getTasksByStatus(models::TaskStatus status)
    {
        return taskDao_->findByStatus(status);
    }

    bool TaskService::updateTaskStatus(int id, models::TaskStatus status)
    {
        auto taskOpt = taskDao_->findById(id);
        if (!taskOpt)
        {
            return false;
        }

        auto task = *taskOpt;
        task.status = status;

        if (status == models::TaskStatus::Running)
        {
            task.started_at = std::chrono::system_clock::now();
        }
        else if (status == models::TaskStatus::Completed || status == models::TaskStatus::Failed)
        {
            task.completed_at = std::chrono::system_clock::now();
        }

        return taskDao_->update(task);
    }

    bool TaskService::updateTaskProgress(int id, int progress, const std::string& message)
    {
        auto taskOpt = taskDao_->findById(id);
        if (!taskOpt)
        {
            return false;
        }

        auto task = *taskOpt;
        task.progress = progress;

        return taskDao_->update(task);
    }

    bool TaskService::retryTask(int id)
    {
        auto taskOpt = taskDao_->findById(id);
        if (!taskOpt)
        {
            return false;
        }

        auto task = *taskOpt;

        if (task.status != models::TaskStatus::Failed)
        {
            return false;
        }

        task.status = models::TaskStatus::Pending;
        task.retry_count++;
        task.progress = 0;
        task.error_message.reset();

        bool success = taskDao_->update(task);

        if (success)
        {
            utils::Logger::info("Task retried: id={}, retry_count={}", id, task.retry_count);
        }

        return success;
    }

    bool TaskService::resumeTask(int id)
    {
        auto taskOpt = taskDao_->findById(id);
        if (!taskOpt)
        {
            return false;
        }

        const auto& task = *taskOpt;

        if (task.status == models::TaskStatus::Completed)
        {
            return false;
        }

        // 对齐 Python resume: 恢复后回到 pending 并清空错误信息;
        // C++ 无 awaiting_confirmation 状态, 允许挂起/失败/超时任务恢复
        bool success = taskDao_->updateStatusAndMessage(id, models::TaskStatus::Pending, 0, "");

        if (success)
        {
            utils::Logger::info("Task resumed: id={}", id);
        }

        return success;
    }

    bool TaskService::forceKillTask(int id)
    {
        auto taskOpt = taskDao_->findById(id);
        if (!taskOpt)
        {
            return false;
        }

        const auto& task = *taskOpt;

        if (task.status == models::TaskStatus::Completed)
        {
            return false;
        }

        // 对齐 Python force-kill: 立即落终态 cancelled 并写错误说明;
        // C++ 无 Celery/子进程, 不做进程终止, 仅归档状态
        bool success = taskDao_->updateStatusAndMessage(id, models::TaskStatus::Cancelled, task.progress,
                                                        "任务卡死, 已强制终结");

        if (success)
        {
            utils::Logger::info("Task force-killed: id={}", id);
        }

        return success;
    }

    bool TaskService::cancelTask(int id)
    {
        auto taskOpt = taskDao_->findById(id);
        if (!taskOpt)
        {
            return false;
        }

        auto task = *taskOpt;

        if (task.status == models::TaskStatus::Completed)
        {
            return false;
        }

        task.status = models::TaskStatus::Cancelled;
        task.completed_at = std::chrono::system_clock::now();

        return taskDao_->update(task);
    }

    bool TaskService::deleteTask(int id)
    {
        return taskDao_->remove(id);
    }

    int TaskService::getActiveTaskCount()
    {
        auto runningTasks = taskDao_->findByStatus(models::TaskStatus::Running);
        auto pendingTasks = taskDao_->findByStatus(models::TaskStatus::Pending);

        return static_cast<int>(runningTasks.size() + pendingTasks.size());
    }

    std::vector<models::Task> TaskService::getRecentTasks(int limit)
    {
        return taskDao_->list(1, limit, "created_at", false);
    }

}  // namespace soilwatercm::services
