#include "ground_truth_controller.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

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

        std::string timestampToIso(const std::chrono::system_clock::time_point& tp)
        {
            if (tp.time_since_epoch().count() == 0)
                return "";
            auto time = std::chrono::system_clock::to_time_t(tp);
            std::tm tm = {};
            localtime_s(&tm, &time);
            std::stringstream ss;
            ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
            return ss.str();
        }

        // 支持 "2024-05-01" / "2024-05-01 10:30:00" / "2024-05-01T10:30:00" 等常见格式
        std::chrono::system_clock::time_point parseDate(const std::string& s)
        {
            static const char* formats[] = {"%Y-%m-%dT%H:%M:%S", "%Y-%m-%d %H:%M:%S", "%Y-%m-%dT%H:%M", "%Y-%m-%d"};
            for (const char* fmt : formats)
            {
                std::tm tm = {};
                std::istringstream ss(s);
                ss >> std::get_time(&tm, fmt);
                if (!ss.fail())
                    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
            }
            return std::chrono::system_clock::time_point{};
        }

        nlohmann::json groundTruthToJson(const models::GroundTruth& gt)
        {
            nlohmann::json j;
            j["id"] = gt.id;
            j["project_id"] = gt.project_id;
            j["period_id"] = gt.period_id.has_value() ? nlohmann::json(*gt.period_id) : nlohmann::json(nullptr);
            j["sample_type"] = gt.sample_type;
            j["location"] = gt.location.wkt;
            j["survey_date"] = timestampToIso(gt.survey_date);
            j["surveyor"] = gt.surveyor.empty() ? nlohmann::json(nullptr) : nlohmann::json(gt.surveyor);
            j["measured_value"] =
                gt.measured_value.has_value() ? nlohmann::json(*gt.measured_value) : nlohmann::json(nullptr);
            j["measured_unit"] = gt.measured_unit.empty() ? nlohmann::json(nullptr) : nlohmann::json(gt.measured_unit);
            j["target_table"] = gt.target_table.empty() ? nlohmann::json(nullptr) : nlohmann::json(gt.target_table);
            j["target_id"] = gt.target_id.has_value() ? nlohmann::json(*gt.target_id) : nlohmann::json(nullptr);
            j["rs_value"] = gt.rs_value.has_value() ? nlohmann::json(*gt.rs_value) : nlohmann::json(nullptr);
            j["error"] = gt.error.has_value() ? nlohmann::json(*gt.error) : nlohmann::json(nullptr);
            j["accuracy_pass"] =
                gt.accuracy_pass.has_value() ? nlohmann::json(*gt.accuracy_pass) : nlohmann::json(nullptr);
            if (!gt.photo_paths.empty())
            {
                try
                {
                    j["photo_paths"] = nlohmann::json::parse(gt.photo_paths);
                }
                catch (const std::exception&)
                {
                    j["photo_paths"] = gt.photo_paths;
                }
            }
            else
            {
                j["photo_paths"] = nullptr;
            }
            j["note"] = gt.note.empty() ? nlohmann::json(nullptr) : nlohmann::json(gt.note);
            j["created_at"] = timestampToIso(gt.created_at);
            return j;
        }
    }  // namespace

    GroundTruthController::GroundTruthController(std::shared_ptr<services::GroundTruthService> groundTruthService)
        : groundTruthService_(std::move(groundTruthService))
    {
    }

    void GroundTruthController::listGroundTruth(const drogon::HttpRequestPtr& req,
                                                std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            auto projectId = parseOptIntParam(req, "project_id");
            auto periodId = parseOptIntParam(req, "period_id");
            std::string sampleType = std::string(req->getParameter("sample_type"));
            int page = parseIntParam(req, "page", 1);
            int pageSize = parseIntParam(req, "page_size", 50);
            if (page < 1)
                page = 1;
            if (pageSize < 1 || pageSize > 500)
                pageSize = 50;

            auto rows = groundTruthService_->list(projectId, periodId, sampleType, page, pageSize);
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& gt : rows)
                arr.push_back(groundTruthToJson(gt));
            callback(jsonResp(arr));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

    void GroundTruthController::createGroundTruth(const drogon::HttpRequestPtr& req,
                                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback)
    {
        try
        {
            auto jsonOpt = req->getJsonObject();
            if (!jsonOpt)
            {
                callback(errorResp("Invalid JSON"));
                return;
            }
            nlohmann::json body = nlohmann::json::parse(req->body());

            models::GroundTruth gt;
            gt.project_id = body.value("project_id", 0);
            if (body.contains("period_id") && body["period_id"].is_number())
                gt.period_id = body["period_id"].get<int>();
            gt.sample_type = body.value("sample_type", "");

            // location: 支持 lng/lat (同 Python) 或 WKT 字符串
            if (body.contains("lng") && body.contains("lat") && body["lng"].is_number() && body["lat"].is_number())
            {
                std::ostringstream oss;
                oss << "POINT(" << body["lng"].get<double>() << " " << body["lat"].get<double>() << ")";
                gt.location.wkt = oss.str();
                gt.location.srid = "EPSG:4326";
            }
            else if (body.contains("location") && body["location"].is_string())
            {
                gt.location.wkt = body["location"].get<std::string>();
                gt.location.srid = "EPSG:4326";
            }

            if (body.contains("survey_date") && body["survey_date"].is_string())
                gt.survey_date = parseDate(body["survey_date"].get<std::string>());
            if (gt.survey_date.time_since_epoch().count() == 0)
                gt.survey_date = std::chrono::system_clock::now();

            if (body.contains("surveyor") && body["surveyor"].is_string())
                gt.surveyor = body["surveyor"].get<std::string>();
            if (body.contains("measured_value") && body["measured_value"].is_number())
                gt.measured_value = body["measured_value"].get<double>();
            if (body.contains("measured_unit") && body["measured_unit"].is_string())
                gt.measured_unit = body["measured_unit"].get<std::string>();
            if (body.contains("target_table") && body["target_table"].is_string())
                gt.target_table = body["target_table"].get<std::string>();
            if (body.contains("target_id") && body["target_id"].is_number())
                gt.target_id = body["target_id"].get<int>();
            if (body.contains("photo_paths"))
            {
                if (body["photo_paths"].is_string())
                    gt.photo_paths = body["photo_paths"].get<std::string>();
                else if (!body["photo_paths"].is_null())
                    gt.photo_paths = body["photo_paths"].dump();
            }
            if (body.contains("note") && body["note"].is_string())
                gt.note = body["note"].get<std::string>();

            auto created = groundTruthService_->create(gt);
            callback(jsonResp(groundTruthToJson(created), drogon::k201Created));
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

    void GroundTruthController::verifyAccuracy(const drogon::HttpRequestPtr& /*req*/,
                                               std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id)
    {
        try
        {
            auto result = groundTruthService_->verifyAccuracy(id);
            callback(jsonResp(result));
        }
        catch (const common::NotFoundException& e)
        {
            callback(errorResp(e.what(), drogon::k404NotFound));
        }
        catch (const std::exception& e)
        {
            callback(errorResp(e.what()));
        }
    }

}  // namespace soilwatercm::controllers
