#include "anomaly_service.h"

#include <chrono>
#include <cmath>

#include "../infrastructure/gdal/gdal_wrapper.h"
#include "../utils/logger.h"

namespace soilwatercm::services
{

    AnomalyService::AnomalyService(std::shared_ptr<dao::AnomalyDao> anomalyDao) : anomalyDao_(std::move(anomalyDao)) {}

    std::vector<models::Anomaly> AnomalyService::detectAnomalies(const AnomalyDetectParams& params)
    {
        std::vector<models::Anomaly> anomalies;

        if (params.type == "disturbance")
        {
            anomalies = anomalyDao_->findByProjectId(params.project_id);
        }
        else if (params.type == "deformation")
        {
            anomalies = anomalyDao_->findByProjectId(params.project_id);
        }

        return anomalies;
    }

    std::vector<models::Anomaly> AnomalyService::detectDisturbanceViolation(
        int projectId, int periodId, const std::vector<models::Disturbance>& disturbances)
    {
        std::vector<models::Anomaly> anomalies;

        for (const auto& disturbance : disturbances)
        {
            if (disturbance.type == models::DisturbanceType::Illegal)
            {
                models::Anomaly anomaly;
                anomaly.type = models::AnomalyType::DisturbanceViolation;
                anomaly.geometry = disturbance.geometry;
                anomaly.severity = models::Severity::High;
                anomaly.description = "违规扰动";
                anomaly.status = models::DisposalStatus::Pending;
                anomaly.project_id = projectId;
                anomaly.period_id = periodId;
                anomaly.detected_at = std::chrono::system_clock::now();

                int id = anomalyDao_->insert(anomaly);
                anomaly.id = id;

                anomalies.push_back(anomaly);
            }
        }

        return anomalies;
    }

    std::vector<models::Anomaly> AnomalyService::detectInsarDeformation(int projectId, int periodId,
                                                                        const std::string& deformationPath,
                                                                        double threshold)
    {
        std::vector<models::Anomaly> anomalies;

        utils::Logger::info("Detecting InSAR deformation anomalies: path={}, threshold={}", deformationPath, threshold);

        try
        {
            infrastructure::gdal::GdalWrapper gdal;
            auto rasterData = gdal.readRaster(deformationPath);

            if (rasterData.data.empty() || rasterData.width == 0 || rasterData.height == 0)
            {
                utils::Logger::warn("InSAR deformation raster is empty: {}", deformationPath);
                return anomalies;
            }

            double cellSizeX = std::abs(rasterData.geotransform[1]);
            double cellSizeY = std::abs(rasterData.geotransform[5]);

            std::vector<std::vector<int>> labeled(rasterData.height, std::vector<int>(rasterData.width, 0));
            int nextLabel = 1;

            for (int row = 0; row < rasterData.height; ++row)
            {
                for (int col = 0; col < rasterData.width; ++col)
                {
                    size_t idx = static_cast<size_t>(row) * rasterData.width + col;
                    double value = rasterData.data[idx];

                    if (std::isnan(value) || std::isinf(value))
                        continue;

                    if (std::abs(value) > threshold)
                    {
                        if (labeled[row][col] == 0)
                        {
                            labeled[row][col] = nextLabel;
                            nextLabel++;
                        }

                        for (int dr = -1; dr <= 1; ++dr)
                        {
                            for (int dc = -1; dc <= 1; ++dc)
                            {
                                int nr = row + dr;
                                int nc = col + dc;
                                if (nr >= 0 && nr < rasterData.height && nc >= 0 && nc < rasterData.width)
                                {
                                    size_t nIdx = static_cast<size_t>(nr) * rasterData.width + nc;
                                    double nVal = rasterData.data[nIdx];
                                    if (!std::isnan(nVal) && !std::isinf(nVal) && std::abs(nVal) > threshold)
                                    {
                                        if (labeled[nr][nc] == 0)
                                        {
                                            labeled[nr][nc] = labeled[row][col];
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            std::map<int, std::vector<std::pair<int, int>>> regions;
            for (int row = 0; row < rasterData.height; ++row)
            {
                for (int col = 0; col < rasterData.width; ++col)
                {
                    if (labeled[row][col] > 0)
                    {
                        regions[labeled[row][col]].push_back({row, col});
                    }
                }
            }

            for (const auto& [label, pixels] : regions)
            {
                if (pixels.size() < 5)
                    continue;

                double sumDeformation = 0.0;
                double maxDeformation = 0.0;
                int minRow = rasterData.height, maxRow = 0;
                int minCol = rasterData.width, maxCol = 0;

                for (const auto& [row, col] : pixels)
                {
                    size_t idx = static_cast<size_t>(row) * rasterData.width + col;
                    double val = rasterData.data[idx];
                    sumDeformation += val;
                    maxDeformation = std::max(maxDeformation, std::abs(val));
                    minRow = std::min(minRow, row);
                    maxRow = std::max(maxRow, row);
                    minCol = std::min(minCol, col);
                    maxCol = std::max(maxCol, col);
                }

                double meanDeformation = sumDeformation / pixels.size();

                double geoMinX = rasterData.geotransform[0] + minCol * cellSizeX;
                double geoMaxY = rasterData.geotransform[3] + minRow * cellSizeY;
                double geoMaxX = rasterData.geotransform[0] + (maxCol + 1) * cellSizeX;
                double geoMinY = rasterData.geotransform[3] + (maxRow + 1) * cellSizeY;

                std::string wkt = "POLYGON((" + std::to_string(geoMinX) + " " + std::to_string(geoMinY) + ", " +
                                  std::to_string(geoMaxX) + " " + std::to_string(geoMinY) + ", " +
                                  std::to_string(geoMaxX) + " " + std::to_string(geoMaxY) + ", " +
                                  std::to_string(geoMinX) + " " + std::to_string(geoMaxY) + ", " +
                                  std::to_string(geoMinX) + " " + std::to_string(geoMinY) + "))";

                models::Anomaly anomaly;
                anomaly.type = models::AnomalyType::InsarDeformation;
                anomaly.geometry = models::Geometry(wkt);
                anomaly.project_id = projectId;
                anomaly.period_id = periodId;
                anomaly.detected_at = std::chrono::system_clock::now();
                anomaly.status = models::DisposalStatus::Pending;

                std::ostringstream desc;
                desc << "InSAR形变异常: 形变区域面积=" << pixels.size() * cellSizeX * cellSizeY
                     << "m², 最大形变=" << maxDeformation << "m"
                     << ", 平均形变=" << meanDeformation << "m";
                anomaly.description = desc.str();

                if (maxDeformation > threshold * 3)
                {
                    anomaly.severity = models::Severity::High;
                }
                else if (maxDeformation > threshold * 2)
                {
                    anomaly.severity = models::Severity::Medium;
                }
                else
                {
                    anomaly.severity = models::Severity::Low;
                }

                int id = anomalyDao_->insert(anomaly);
                anomaly.id = id;
                anomalies.push_back(anomaly);
            }

            utils::Logger::info("InSAR deformation detection completed: {} anomalies found", anomalies.size());
        }
        catch (const std::exception& e)
        {
            utils::Logger::error("InSAR deformation detection failed: {}", e.what());
        }

        return anomalies;
    }

    bool AnomalyService::sendAlert(const models::Anomaly& anomaly)
    {
        utils::Logger::info("Sending alert for anomaly: id={}", anomaly.id);

        return true;
    }

    bool AnomalyService::sendEmail(const std::string& to, const std::string& subject, const std::string& body)
    {
        utils::Logger::info("Sending email: to={}, subject={}", to, subject);

        return true;
    }

    bool AnomalyService::sendSms(const std::string& phone, const std::string& message)
    {
        utils::Logger::info("Sending SMS: phone={}", phone);

        return true;
    }

    bool AnomalyService::assignAnomaly(int anomalyId, int assigneeId)
    {
        return anomalyDao_->updateAssignee(anomalyId, assigneeId);
    }

    bool AnomalyService::dispatchAnomaly(int anomalyId)
    {
        auto anomalyOpt = anomalyDao_->findById(anomalyId);
        if (!anomalyOpt)
        {
            return false;
        }

        auto anomaly = *anomalyOpt;

        if (anomaly.status == models::DisposalStatus::Completed)
        {
            return false;
        }

        // 对齐 Python dispatch(无人机详查派单): 置为已派单状态。
        // C++ DisposalStatus 无 dispatched, 映射为 Assigned(已分配处置)
        anomaly.status = models::DisposalStatus::Assigned;

        return anomalyDao_->updateDetails(anomaly);
    }

    bool AnomalyService::notifyAnomaly(int anomalyId)
    {
        auto anomalyOpt = anomalyDao_->findById(anomalyId);
        if (!anomalyOpt)
        {
            return false;
        }

        // 对齐 Python notify: 按通道推送预警。C++ 无短信/邮件网关, sendAlert 记录日志并返回占位成功
        return sendAlert(*anomalyOpt);
    }

    bool AnomalyService::updateAnomaly(int anomalyId, const std::optional<models::DisposalStatus>& status,
                                       const std::optional<models::Severity>& severity,
                                       const std::optional<std::string>& description,
                                       const std::optional<std::string>& measures,
                                       const std::optional<std::string>& assignee)
    {
        auto anomalyOpt = anomalyDao_->findById(anomalyId);
        if (!anomalyOpt)
        {
            return false;
        }

        auto anomaly = *anomalyOpt;

        if (status)
        {
            anomaly.status = *status;
            // 对齐 Python: 状态置为已解决时记录解决时间
            if (*status == models::DisposalStatus::Completed && !anomaly.resolved_at)
            {
                anomaly.resolved_at = std::chrono::system_clock::now();
            }
        }
        if (severity)
        {
            anomaly.severity = *severity;
        }
        if (description)
        {
            anomaly.description = *description;
        }
        if (measures)
        {
            anomaly.measures = measures;
        }
        if (assignee)
        {
            anomaly.assignee = assignee;
        }

        return anomalyDao_->updateDetails(anomaly);
    }

    bool AnomalyService::resolveAnomaly(int anomalyId, const std::string& measures)
    {
        auto anomalyOpt = anomalyDao_->findById(anomalyId);
        if (!anomalyOpt)
        {
            return false;
        }

        auto anomaly = *anomalyOpt;
        anomaly.status = models::DisposalStatus::Resolved;
        anomaly.measures = measures;
        anomaly.resolved_at = std::chrono::system_clock::now();

        return anomalyDao_->update(anomaly);
    }

    std::vector<models::Anomaly> AnomalyService::getAnomaliesByProject(int projectId)
    {
        return anomalyDao_->findByProjectId(projectId);
    }

    std::vector<models::Anomaly> AnomalyService::getAnomaliesByStatus(models::DisposalStatus status)
    {
        return anomalyDao_->findByStatus(status);
    }

    std::optional<models::Anomaly> AnomalyService::getAnomalyById(int id)
    {
        return anomalyDao_->findById(id);
    }

}  // namespace soilwatercm::services
