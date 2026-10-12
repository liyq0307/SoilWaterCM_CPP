#include "fvc_service.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

#include "../common/exceptions.h"
#include "../utils/logger.h"

namespace soilwatercm::services
{

    FvcService::FvcService(std::shared_ptr<dao::VegetationCoverageDao> fvcDao, std::shared_ptr<dao::ImageDao> imageDao,
                           std::shared_ptr<dao::TaskDao> taskDao,
                           std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal,
                           std::shared_ptr<infrastructure::gdal::RasterProcessor> rasterProcessor)
        : fvcDao_(std::move(fvcDao)),
          imageDao_(std::move(imageDao)),
          taskDao_(std::move(taskDao)),
          gdal_(std::move(gdal)),
          rasterProcessor_(std::move(rasterProcessor))
    {
    }

    std::future<FvcResult> FvcService::submitFvcTask(const FvcParams& params)
    {
        return std::async(std::launch::async,
                          [this, params]()
                          {
                              models::Task task;
                              task.type = models::TaskType::FvcExtraction;
                              task.status = models::TaskStatus::Pending;
                              task.submitted_at = std::chrono::system_clock::now();
                              task.period_id = params.period_id;

                              int taskId = taskDao_->insert(task);

                              return this->executeFvc(params, taskId);
                          });
    }

    FvcResult FvcService::executeFvc(const FvcParams& params, int taskId)
    {
        FvcResult result;
        result.task_id = taskId;

        try
        {
            updateTaskProgress(taskId, 0, "开始FVC反演");

            std::filesystem::create_directories(params.output_dir);

            if (params.image_ids.empty())
            {
                throw common::ValidationException("未指定影像");
            }

            auto imageOpt = imageDao_->findById(params.image_ids[0]);
            if (!imageOpt)
            {
                throw common::NotFoundException("影像不存在");
            }

            std::string inputPath = imageOpt->file_path;

            updateTaskProgress(taskId, 20, "计算NDVI");

            std::string redBandPath = inputPath;
            std::string nirBandPath = inputPath;
            std::string ndviPath = generateOutputPath(params.output_dir, "_ndvi.tif");

            NdviResult ndviResult = calculateNdvi(redBandPath, nirBandPath, ndviPath);
            if (!ndviResult.success)
            {
                throw common::ProcessingException(ndviResult.message);
            }

            updateTaskProgress(taskId, 50, "计算FVC");

            std::string fvcPath = generateOutputPath(params.output_dir, "_fvc.tif");

            double ndviSoil = params.ndvi_soil.value_or(0.05);
            double ndviVeg = params.ndvi_veg.value_or(0.70);

            FvcResult fvcCalcResult = calculateFvc(ndviPath, fvcPath, ndviSoil, ndviVeg);
            if (!fvcCalcResult.success)
            {
                throw common::ProcessingException(fvcCalcResult.message);
            }

            updateTaskProgress(taskId, 70, "计算统计信息");

            auto stats = calculateStatistics(fvcPath);

            double meanFvc = 0.0;
            if (stats.find("mean") != stats.end())
            {
                meanFvc = stats["mean"];
            }

            updateTaskProgress(taskId, 90, "保存结果");

            models::VegetationCoverage fvc;
            fvc.fvc_raster_path = fvcPath;
            fvc.mean_fvc = meanFvc;
            fvc.period_id = params.period_id;
            fvc.created_at = std::chrono::system_clock::now();

            int fvcId = fvcDao_->insert(fvc);
            fvc.id = fvcId;

            result.success = true;
            result.message = "FVC反演完成";
            result.fvc_raster_path = fvcPath;
            result.mean_fvc = meanFvc;
            result.class_areas = stats;

            updateTaskProgress(taskId, 100, "FVC反演完成");

            utils::Logger::info("FVC extraction completed: period={}, mean_fvc={}", params.period_id, meanFvc);
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.message = std::string("FVC反演失败: ") + e.what();
            updateTaskProgress(taskId, -1, result.message);
        }

        return result;
    }

    NdviResult FvcService::calculateNdvi(const std::string& redBandPath, const std::string& nirBandPath,
                                         const std::string& outputPath)
    {
        NdviResult result;

        try
        {
            auto redData = gdal_->readRaster(redBandPath);
            auto nirData = gdal_->readRaster(nirBandPath);

            if (redData.data.size() != nirData.data.size())
            {
                result.success = false;
                result.message = "红光波段和近红外波段尺寸不匹配";
                return result;
            }

            std::vector<double> ndviData(redData.data.size());

            for (size_t i = 0; i < redData.data.size(); ++i)
            {
                double red = redData.data[i];
                double nir = nirData.data[i];

                double denominator = nir + red;
                if (std::abs(denominator) < 1e-10)
                {
                    ndviData[i] = 0.0;
                }
                else
                {
                    ndviData[i] = (nir - red) / denominator;
                    ndviData[i] = std::max(-1.0, std::min(1.0, ndviData[i]));
                }
            }

            infrastructure::gdal::RasterData outputData;
            outputData.data = ndviData;
            outputData.width = redData.width;
            outputData.height = redData.height;
            outputData.geotransform = redData.geotransform;
            outputData.projection = redData.projection;

            gdal_->writeRaster(outputPath, outputData);

            result.success = true;
            result.message = "NDVI计算完成";
            result.ndvi_raster_path = outputPath;
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.message = std::string("NDVI计算失败: ") + e.what();
        }

        return result;
    }

    FvcResult FvcService::calculateFvc(const std::string& ndviPath, const std::string& outputPath, double ndviSoil,
                                       double ndviVeg)
    {
        FvcResult result;

        try
        {
            auto ndviData = gdal_->readRaster(ndviPath);

            std::vector<double> fvcData(ndviData.data.size());

            double denominator = ndviVeg - ndviSoil;
            if (std::abs(denominator) < 1e-10)
            {
                result.success = false;
                result.message = "NDVI土壤和植被参数差异过小";
                return result;
            }

            for (size_t i = 0; i < ndviData.data.size(); ++i)
            {
                double ndvi = ndviData.data[i];

                double fvc = (ndvi - ndviSoil) / denominator;

                fvc = std::max(0.0, std::min(1.0, fvc));

                fvcData[i] = fvc;
            }

            infrastructure::gdal::RasterData outputData;
            outputData.data = fvcData;
            outputData.width = ndviData.width;
            outputData.height = ndviData.height;
            outputData.geotransform = ndviData.geotransform;
            outputData.projection = ndviData.projection;

            gdal_->writeRaster(outputPath, outputData);

            result.success = true;
            result.message = "FVC计算完成";
            result.fvc_raster_path = outputPath;
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.message = std::string("FVC计算失败: ") + e.what();
        }

        return result;
    }

    int FvcService::classifyFvcValue(double fvc)
    {
        if (fvc < 0.2)
            return 1;
        if (fvc < 0.4)
            return 2;
        if (fvc < 0.6)
            return 3;
        if (fvc < 0.8)
            return 4;
        return 5;
    }

    std::map<std::string, double> FvcService::calculateStatistics(const std::string& fvcPath)
    {
        std::map<std::string, double> stats;

        try
        {
            auto fvcData = gdal_->readRaster(fvcPath);

            double sum = 0.0;
            int count = 0;

            std::map<int, int> classCounts;

            for (double value : fvcData.data)
            {
                if (std::isnan(value) || std::isinf(value))
                {
                    continue;
                }

                sum += value;
                count++;

                int fvcClass = classifyFvcValue(value);
                classCounts[fvcClass]++;
            }

            if (count > 0)
            {
                stats["mean"] = sum / count;
            }

            stats["total_pixels"] = static_cast<double>(count);

            for (const auto& [fvcClass, classCount] : classCounts)
            {
                std::string key = "class_" + std::to_string(fvcClass);
                stats[key] = static_cast<double>(classCount);
            }
        }
        catch (const std::exception& e)
        {
            utils::Logger::warn("FVC统计计算失败: {}", e.what());
        }

        return stats;
    }

    std::optional<models::VegetationCoverage> FvcService::getFvcByPeriod(int periodId)
    {
        return fvcDao_->findByPeriodId(periodId);
    }

    std::vector<models::VegetationCoverage> FvcService::listFvc(int page, int pageSize)
    {
        return fvcDao_->list(page, pageSize);
    }

    void FvcService::updateTaskProgress(int taskId, int progress, const std::string& message)
    {
        auto taskOpt = taskDao_->findById(taskId);
        if (!taskOpt)
        {
            return;
        }

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

    std::string FvcService::generateOutputPath(const std::string& baseDir, const std::string& suffix)
    {
        return (std::filesystem::path(baseDir) / ("result" + suffix)).string();
    }

}  // namespace soilwatercm::services
