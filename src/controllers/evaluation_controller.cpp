#include "evaluation_controller.h"

#include <drogon/HttpTypes.h>

namespace soilwatercm::controllers
{

    EvaluationController::EvaluationController(std::shared_ptr<services::EvaluationService> evaluationService)
        : evaluationService_(std::move(evaluationService))
    {
    }

    void EvaluationController::evaluate(const drogon::HttpRequestPtr& req,
                                        std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        auto json = req->getJsonObject();
        if (!json)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Invalid JSON"})");
            callback(resp);
            return;
        }

        services::EvaluationParams params;
        params.period_id = (*json)["period_id"].asInt();

        auto future = evaluationService_->submitEvaluationTask(params);

        Json::Value result;
        result["message"] = "Evaluation task submitted";
        result["status"] = "pending";

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k202Accepted);
        callback(resp);
    }

    void EvaluationController::getResultByQuery(const drogon::HttpRequestPtr& req,
                                                std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        std::string periodIdStr = std::string(req->getParameter("period_id"));

        if (periodIdStr.empty())
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "period_id is required"})");
            callback(resp);
            return;
        }

        int periodId = std::stoi(periodIdStr);
        auto evalOpt = evaluationService_->getEvaluationByPeriod(periodId);

        if (!evalOpt)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Evaluation not found"})");
            callback(resp);
            return;
        }

        const auto& evaluation = *evalOpt;

        Json::Value result;
        result["id"] = evaluation.id;
        result["period_id"] = evaluation.period_id;
        result["compliance_score"] = evaluation.compliance_score;
        result["compliance_level"] = static_cast<int>(evaluation.compliance_level);
        result["risk_score"] = evaluation.risk_score;
        result["risk_level"] = static_cast<int>(evaluation.risk_level);

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

    void EvaluationController::getResult(const drogon::HttpRequestPtr& /*req*/,
                                         std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        auto evalOpt = evaluationService_->getEvaluationByPeriod(id);

        if (!evalOpt)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(R"({"error": "Evaluation not found"})");
            callback(resp);
            return;
        }

        const auto& evaluation = *evalOpt;

        Json::Value result;
        result["id"] = evaluation.id;
        result["compliance_score"] = evaluation.compliance_score;
        result["compliance_level"] = static_cast<int>(evaluation.compliance_level);
        result["risk_score"] = evaluation.risk_score;
        result["risk_level"] = static_cast<int>(evaluation.risk_level);

        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }

}  // namespace soilwatercm::controllers
