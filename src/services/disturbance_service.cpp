#include "disturbance_service.h"

#include <filesystem>

#include "../common/exceptions.h"
#include "../utils/logger.h"

namespace soilwatercm::services
{

    DisturbanceService::DisturbanceService(std::shared_ptr<dao::DisturbanceDao> disturbanceDao,
                                           std::shared_ptr<dao::TaskDao> taskDao,
                                           std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal,
                                           std::shared_ptr<infrastructure::gdal::RasterProcessor> rasterProcessor,
                                           std::shared_ptr<infrastructure::gdal::VectorProcessor> vectorProcessor,
                                           std::shared_ptr<infrastructure::OtbInterface> otb,
                                           std::shared_ptr<infrastructure::external_tools::GrassInterface> grass)
        : disturbanceDao_(std::move(disturbanceDao)),
          taskDao_(std::move(taskDao)),
          gdal_(std::move(gdal)),
          rasterProcessor_(std::move(rasterProcessor)),
          vectorProcessor_(std::move(vectorProcessor)),
          otb_(std::move(otb)),
          grass_(std::move(grass))
    {
    }

    std::future<DisturbanceResult> DisturbanceService::submitDisturbanceTask(const DisturbanceParams& params)
    {
        return std::async(std::launch::async,
                          [this, params]()
                          {
                              models::Task task;
                              task.type = models::TaskType::DisturbanceExtraction;
                              task.status = models::TaskStatus::Pending;
                              task.submitted_at = std::chrono::system_clock::now();
                              task.period_id = params.period_id;

                              int taskId = taskDao_->insert(task);

                              return this->executeDisturbance(params, taskId);
                          });
    }

    DisturbanceResult DisturbanceService::executeDisturbance(const DisturbanceParams& params, int taskId)
    {
        DisturbanceResult result;
        result.task_id = taskId;

        try
        {
            updateTaskProgress(taskId, 0, "开始扰动图斑提取");

            std::filesystem::create_directories(params.output_dir);

            updateTaskProgress(taskId, 20, "变化检测");
            std::string changePath = (std::filesystem::path(params.output_dir) / "change.tif").string();
            changeDetection(params.before_image_path, params.after_image_path, changePath);

            updateTaskProgress(taskId, 40, "扰动分类");
            std::string classPath = (std::filesystem::path(params.output_dir) / "disturbance_class.tif").string();
            classifyDisturbance(changePath, classPath);

            updateTaskProgress(taskId, 60, "栅格转矢量");
            auto geometries = rasterToVector(classPath);

            updateTaskProgress(taskId, 80, "创建扰动图斑");
            std::vector<models::Disturbance> disturbances;

            for (size_t i = 0; i < geometries.size(); ++i)
            {
                models::Disturbance disturbance;
                disturbance.disturbance_code = "DIST_" + std::to_string(params.period_id) + "_" + std::to_string(i);
                disturbance.type = models::DisturbanceType::Construction;
                disturbance.geometry = geometries[i];
                disturbance.area = gdal_->calculateArea(geometries[i].wkt);
                disturbance.confidence = params.confidence_threshold;
                disturbance.period_id = params.period_id;
                disturbance.created_at = std::chrono::system_clock::now();

                if (disturbance.area >= params.min_area)
                {
                    int id = disturbanceDao_->insert(disturbance);
                    disturbance.id = id;
                    disturbances.push_back(disturbance);
                }
            }

            result.success = true;
            result.message = "扰动图斑提取完成";
            result.disturbances = disturbances;
            result.total_count = static_cast<int>(disturbances.size());

            double totalArea = 0.0;
            for (const auto& d : disturbances)
            {
                totalArea += d.area;
            }
            result.total_area = totalArea;

            updateTaskProgress(taskId, 100, "扰动图斑提取完成");

            utils::Logger::info("Disturbance extraction completed: period={}, count={}", params.period_id,
                                result.total_count);
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.message = std::string("扰动图斑提取失败: ") + e.what();
            updateTaskProgress(taskId, -1, result.message);
        }

        return result;
    }

    std::string DisturbanceService::changeDetection(const std::string& beforePath, const std::string& afterPath,
                                                    const std::string& outputPath)
    {
        if (otb_ && otb_->isAvailable())
        {
            otb_->changeDetection(beforePath, afterPath, outputPath);
            return outputPath;
        }

        auto beforeData = gdal_->readRaster(beforePath);
        auto afterData = gdal_->readRaster(afterPath);

        std::vector<double> changeData(beforeData.data.size());
        for (size_t i = 0; i < beforeData.data.size(); ++i)
        {
            changeData[i] = std::abs(afterData.data[i] - beforeData.data[i]);
        }

        infrastructure::gdal::RasterData outputData;
        outputData.data = changeData;
        outputData.width = beforeData.width;
        outputData.height = beforeData.height;
        outputData.geotransform = beforeData.geotransform;
        outputData.projection = beforeData.projection;

        gdal_->writeRaster(outputPath, outputData);
        return outputPath;
    }

    std::string DisturbanceService::classifyDisturbance(const std::string& changePath, const std::string& outputPath)
    {
        auto changeData = gdal_->readRaster(changePath);

        std::vector<double> classData(changeData.data.size());

        double maxVal = *std::max_element(changeData.data.begin(), changeData.data.end());
        double threshold = (maxVal > 0) ? maxVal * 0.3 : 0.3;

        for (size_t i = 0; i < changeData.data.size(); ++i)
        {
            classData[i] = (changeData.data[i] > threshold) ? 1.0 : 0.0;
        }

        infrastructure::gdal::RasterData outputData;
        outputData.data = classData;
        outputData.width = changeData.width;
        outputData.height = changeData.height;
        outputData.geotransform = changeData.geotransform;
        outputData.projection = changeData.projection;

        gdal_->writeRaster(outputPath, outputData);
        return outputPath;
    }

    std::vector<models::Geometry> DisturbanceService::rasterToVector(const std::string& rasterPath)
    {
        return vectorProcessor_->rasterToVector(rasterPath);
    }

    std::string DisturbanceService::morphologyProcess(const std::string& vectorPath, const std::string& outputPath,
                                                      const std::string& operation)
    {
        if (grass_ && grass_->isAvailable())
        {
            grass_->morphologyProcess(vectorPath, outputPath, operation);
        }
        else
        {
            return vectorPath;
        }
        return outputPath;
    }

    std::vector<models::Disturbance> DisturbanceService::filterSmallPolygons(
        std::vector<models::Disturbance>& disturbances, double minArea)
    {
        std::vector<models::Disturbance> filtered;

        for (auto& d : disturbances)
        {
            if (d.area >= minArea)
            {
                filtered.push_back(std::move(d));
            }
        }

        return filtered;
    }

    std::vector<models::Disturbance> DisturbanceService::getDisturbancesByPeriod(int periodId)
    {
        return disturbanceDao_->findByPeriodId(periodId);
    }

    std::vector<models::Disturbance> DisturbanceService::getDisturbancesByProject(int projectId,
                                                                                  std::optional<int> periodId)
    {
        return disturbanceDao_->findByProject(projectId, periodId);
    }

    int DisturbanceService::createDisturbance(const models::Disturbance& disturbance)
    {
        return disturbanceDao_->insert(disturbance);
    }

    std::optional<models::Disturbance> DisturbanceService::getDisturbanceById(int id)
    {
        return disturbanceDao_->findById(id);
    }

    bool DisturbanceService::updateDisturbance(const models::Disturbance& disturbance)
    {
        return disturbanceDao_->update(disturbance);
    }

    bool DisturbanceService::deleteDisturbance(int id)
    {
        return disturbanceDao_->remove(id);
    }

    void DisturbanceService::updateTaskProgress(int taskId, int progress, const std::string& message)
    {
        auto taskOpt = taskDao_->findById(taskId);
        if (!taskOpt)
            return;

        auto task = *taskOpt;
        task.progress = progress;

        if (progress == 100)
        {
            task.status = models::TaskStatus::Completed;
            task.completed_at = std::chrono::system_clock::now();
        }
        else if (progress < 0)
        {
            task.status = models::TaskStatus::Failed;
            task.error_message = message;
        }
        else if (progress == 0)
        {
            task.status = models::TaskStatus::Running;
            task.started_at = std::chrono::system_clock::now();
        }

        taskDao_->update(task);
    }

}  // namespace soilwatercm::services
