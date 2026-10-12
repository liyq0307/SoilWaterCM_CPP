#include "indicator_controller.h"

#include <iomanip>
#include <sstream>

#include <drogon/HttpTypes.h>

#include "../common/exceptions.h"
#include "../utils/logger.h"
#include <nlohmann/json.hpp>

namespace soilwatercm::controllers
{

    namespace
    {
        drogon::HttpResponsePtr jsonResp(const nlohmann::json& body, drogon::HttpStatusCode code = drogon::k200OK)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(code);
            resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            resp->setBody(body.dump());
            return resp;
        }

        drogon::HttpResponsePtr errorResp(const std::string& msg, drogon::HttpStatusCode code = drogon::k400BadRequest)
        {
            nlohmann::json j;
            j["error"] = msg;
            return jsonResp(j, code);
        }

        int parseIntParam(const drogon::HttpRequestPtr& req, const std::string& key, int fallback = 0)
        {
            std::string v = std::string(req->getParameter(key));
            if (v.empty())
                return fallback;
            try
            {
                return std::stoi(v);
            }
            catch (const std::exception&)
            {
                return fallback;
            }
        }

        std::optional<int> parseOptIntParam(const drogon::HttpRequestPtr& req, const std::string& key)
        {
            std::string v = std::string(req->getParameter(key));
            if (v.empty())
                return std::nullopt;
            try
            {
                return std::stoi(v);
            }
            catch (const std::exception&)
            {
                return std::nullopt;
            }
        }

        std::string timestampToJson(const std::chrono::system_clock::time_point& tp)
        {
            auto time = std::chrono::system_clock::to_time_t(tp);
            std::tm tm = {};
            localtime_s(&tm, &time);
            std::stringstream ss;
            ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
            return ss.str();
        }

        nlohmann::json indicatorToJson(const models::Indicator& ind)
        {
            nlohmann::json j;
            j["id"] = ind.id;
            j["code"] = ind.code;
            j["name"] = ind.name;
            j["dimension"] = ind.dimension;
            j["unit"] = ind.unit.has_value() ? nlohmann::json(*ind.unit) : nlohmann::json(nullptr);
            j["description"] = ind.description.has_value() ? nlohmann::json(*ind.description) : nlohmann::json(nullptr);
            j["weight"] = ind.weight;
            j["threshold_good"] =
                ind.threshold_good.has_value() ? nlohmann::json(*ind.threshold_good) : nlohmann::json(nullptr);
            j["threshold_warn"] =
                ind.threshold_warn.has_value() ? nlohmann::json(*ind.threshold_warn) : nlohmann::json(nullptr);
            j["threshold_bad"] =
                ind.threshold_bad.has_value() ? nlohmann::json(*ind.threshold_bad) : nlohmann::json(nullptr);
            j["direction"] = ind.direction;
            j["is_active"] = ind.is_active;
            return j;
        }

        nlohmann::json valueToJson(const models::IndicatorValue& v)
        {
            nlohmann::json j;
            j["id"] = v.id;
            j["project_id"] = v.project_id;
            j["period_id"] = v.period_id;
            j["indicator_id"] = v.indicator_id;
            j["raw_value"] = v.raw_value;
            j["normalized_value"] =
                v.normalized_value.has_value() ? nlohmann::json(*v.normalized_value) : nlohmann::json(nullptr);
            j["score"] = v.score.has_value() ? nlohmann::json(*v.score) : nlohmann::json(nullptr);
            j["level"] = v.level.empty() ? nlohmann::json(nullptr) : nlohmann::json(v.level);
            j["created_at"] = timestampToJson(v.created_at);
            return j;
        }

        std::optional<double> parseOptDoubleBody(const nlohmann::json& body, const std::string& key)
        {
            if (!body.contains(key) || body[key].is_null())
                return std::nullopt;
            if (!body[key].is_number())
                throw common::ValidationException("字段 " + key + " 必须为数值");
            return body[key].get<double>();
        }
    }  // namespace

    IndicatorController::IndicatorController(std::shared_ptr<services::IndicatorService> indicatorService)
        : indicatorService_(std::move(indicatorService))
    {
    }

    void IndicatorController::listIndicators(const drogon::HttpRequestPtr& req,
                                             std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            std::string dimension = std::string(req->getParameter("dimension"));
            auto rows = indicatorService_->listIndicators(dimension);
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& ind : rows)
                arr.push_back(indicatorToJson(ind));
            callback(jsonResp(arr));
        }
        catch (const common::ValidationException& e)
        {
            callback(errorResp(e.what()));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void IndicatorController::updateWeight(const drogon::HttpRequestPtr& req,
                                           std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        try
        {
            nlohmann::json body = nlohmann::json::parse(req->body());
            if (!body.contains("weight") || !body["weight"].is_number())
            {
                callback(errorResp("缺少 weight 字段或类型错误"));
                return;
            }
            double weight = body["weight"].get<double>();

            auto updated = indicatorService_->setWeight(id, weight);

            nlohmann::json result;
            result["id"] = updated.id;
            result["code"] = updated.code;
            result["weight"] = updated.weight;
            callback(jsonResp(result));
        }
        catch (const common::NotFoundException& e)
        {
            callback(errorResp(e.what(), drogon::k404NotFound));
        }
        catch (const common::ValidationException& e)
        {
            callback(errorResp(e.what()));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void IndicatorController::updateThresholds(const drogon::HttpRequestPtr& req,
                                               std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        try
        {
            nlohmann::json body = nlohmann::json::parse(req->body());
            auto good = parseOptDoubleBody(body, "threshold_good");
            auto warn = parseOptDoubleBody(body, "threshold_warn");
            auto bad = parseOptDoubleBody(body, "threshold_bad");

            auto updated = indicatorService_->setThresholds(id, good, warn, bad);

            nlohmann::json result;
            result["id"] = updated.id;
            result["thresholds"] = {good.has_value() ? nlohmann::json(*good) : nlohmann::json(nullptr),
                                    warn.has_value() ? nlohmann::json(*warn) : nlohmann::json(nullptr),
                                    bad.has_value() ? nlohmann::json(*bad) : nlohmann::json(nullptr)};
            callback(jsonResp(result));
        }
        catch (const common::NotFoundException& e)
        {
            callback(errorResp(e.what(), drogon::k404NotFound));
        }
        catch (const common::ValidationException& e)
        {
            callback(errorResp(e.what()));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void IndicatorController::listIndicatorValues(const drogon::HttpRequestPtr& req,
                                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            int projectId = parseIntParam(req, "project_id");
            auto periodId = parseOptIntParam(req, "period_id");
            auto indicatorId = parseOptIntParam(req, "indicator_id");

            auto rows = indicatorService_->listValues(projectId, periodId, indicatorId);
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& v : rows)
                arr.push_back(valueToJson(v));
            callback(jsonResp(arr));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

}  // namespace soilwatercm::controllers
