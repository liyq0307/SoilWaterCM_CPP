#include "task_controller.h"

#include <drogon/HttpTypes.h>

namespace soilwatercm::controllers
{

    TaskController::TaskController(std::shared_ptr<services::TaskService> taskService)
        : taskService_(std::move(taskService))
    {
    }

    void TaskController::listTasks(const drogon::HttpRequestPtr& req,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        std::string statusStr = std::string(req->getParameter("status"));
        std::string projectIdStr = std::string(req->getParameter("project_id"));

        std::vector<models::Task> tasks;

        if (!statusStr.empty())
        {
            int statusInt = std::stoi(statusStr);
            models::TaskStatus status = static_cast<models::TaskStatus>(statusInt);
            tasks = taskService_->getTasksByStatus(status);
        }
        else if (!projectIdStr.empty())
        {
            int projectId = std::stoi(projectIdStr);
            tasks = taskService_->getTasksByProject(projectId);
        }
        else
        {
            tasks = taskService_->getRecentTasks(100);
        }

        Json::Value result(Json::arrayValue);
        for (const auto& task : tasks)
        {
            Json::Value item;
            item["id"] = task.id;
            item["type"] = static_cast<int>(task.type);
            item["status"] = static_cast<int>(task.status);
            item["progress"] = task.progress;
            item["project_id"] = task.project_id;
            item["period_id"] = task.period_id;
            result.append(item);
        }

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void TaskController::getTask(const drogon::HttpRequestPtr& /*req*/,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        auto taskOpt = taskService_->getTaskById(id);

        if (!taskOpt)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Task not found"})");
            callback(resp);
            return;
        }

        const auto& task = *taskOpt;

        Json::Value result;
        result["id"] = task.id;
        result["type"] = static_cast<int>(task.type);
        result["status"] = static_cast<int>(task.status);
        result["progress"] = task.progress;
        result["params"] = task.params;
        result["result_path"] = task.result_path.value_or("");
        result["error_message"] = task.error_message.value_or("");
        result["retry_count"] = task.retry_count;
        result["project_id"] = task.project_id;
        result["period_id"] = task.period_id;

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void TaskController::retryTask(const drogon::HttpRequestPtr& /*req*/,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        bool success = taskService_->retryTask(id);

        Json::Value result;
        result["success"] = success;
        result["message"] = success ? "Task retried" : "Retry failed";

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void TaskController::resumeTask(const drogon::HttpRequestPtr& /*req*/,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        bool success = taskService_->resumeTask(id);

        if (!success)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k409Conflict);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Resume failed: task not found or already completed"})");
            callback(resp);
            return;
        }

        Json::Value result;
        result["id"] = id;
        result["status"] = static_cast<int>(models::TaskStatus::Pending);
        result["message"] = "Task resumed";

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void TaskController::forceKillTask(const drogon::HttpRequestPtr& /*req*/,
                                       std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        bool success = taskService_->forceKillTask(id);

        if (!success)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k409Conflict);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Force kill failed: task not found or already in terminal state"})");
            callback(resp);
            return;
        }

        Json::Value result;
        result["id"] = id;
        result["status"] = static_cast<int>(models::TaskStatus::Cancelled);
        result["message"] = "Task force-killed (状态已归档, C++ 后端无进程管理, 不做进程终止)";

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void TaskController::cancelTask(const drogon::HttpRequestPtr& /*req*/,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        bool success = taskService_->cancelTask(id);

        Json::Value result;
        result["success"] = success;
        result["message"] = success ? "Task cancelled" : "Cancel failed";

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void TaskController::deleteTask(const drogon::HttpRequestPtr& /*req*/,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        bool success = taskService_->deleteTask(id);

        if (success)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k204NoContent);
            callback(resp);
        }
        else
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Delete failed"})");
            callback(resp);
        }
    }

}  // namespace soilwatercm::controllers
