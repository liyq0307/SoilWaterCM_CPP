#define _USE_MATH_DEFINES
#include "erosion_service.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>
#include <numeric>

#include "../common/exceptions.h"
#include "../utils/logger.h"

namespace soilwatercm::services
{

    ErosionService::ErosionService(std::shared_ptr<dao::ErosionResultDao> erosionDao,
                                   std::shared_ptr<dao::TaskDao> taskDao,
                                   std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal,
                                   std::shared_ptr<infrastructure::gdal::RasterProcessor> rasterProcessor,
                                   std::shared_ptr<infrastructure::external_tools::GrassInterface> grass,
                                   std::shared_ptr<infrastructure::SagaInterface> saga)
        : erosionDao_(std::move(erosionDao)),
          taskDao_(std::move(taskDao)),
          gdal_(std::move(gdal)),
          rasterProcessor_(std::move(rasterProcessor)),
          grass_(std::move(grass)),
          saga_(std::move(saga))
    {
    }

    std::future<ErosionResult> ErosionService::submitErosionTask(const ErosionParams& params)
    {
        return std::async(std::launch::async,
                          [this, params]()
                          {
                              models::Task task;
                              task.type = models::TaskType::ErosionCalculation;
                              task.status = models::TaskStatus::Pending;
                              task.submitted_at = std::chrono::system_clock::now();
                              task.period_id = params.period_id;

                              int taskId = taskDao_->insert(task);

                              return this->executeErosion(params, taskId);
                          });
    }

    ErosionResult ErosionService::executeErosion(const ErosionParams& params, int taskId)
    {
        ErosionResult result;
        result.task_id = taskId;

        try
        {
            updateTaskProgress(taskId, 0, "开始USLE侵蚀计算");

            std::filesystem::create_directories(params.output_dir);

            updateTaskProgress(taskId, 10, "计算R因子");
            std::string rPath = calculateRFactor(params.rainfall_path,
                                                 (std::filesystem::path(params.output_dir) / "r_factor.tif").string());

            updateTaskProgress(taskId, 20, "计算K因子");
            std::string kPath = calculateKFactor(params.soil_path,
                                                 (std::filesystem::path(params.output_dir) / "k_factor.tif").string());

            updateTaskProgress(taskId, 30, "计算LS因子");
            std::string lsMethod = params.ls_method.value_or("grass");
            std::string lsPath = calculateLSFactor(
                params.dem_path, (std::filesystem::path(params.output_dir) / "ls_factor.tif").string(), lsMethod);

            updateTaskProgress(taskId, 40, "计算C因子");
            std::string cPath =
                calculateCFactor(params.fvc_path, (std::filesystem::path(params.output_dir) / "c_factor.tif").string());

            updateTaskProgress(taskId, 50, "计算P因子");
            std::string pPath = calculatePFactor((std::filesystem::path(params.output_dir) / "p_factor.tif").string());

            updateTaskProgress(taskId, 60, "计算USLE");
            std::string modulusPath =
                calculateUSLE(rPath, kPath, lsPath, cPath, pPath,
                              (std::filesystem::path(params.output_dir) / "erosion_modulus.tif").string());

            updateTaskProgress(taskId, 70, "侵蚀分级");
            std::string classPath =
                classifyErosion(modulusPath, (std::filesystem::path(params.output_dir) / "erosion_class.tif").string());

            updateTaskProgress(taskId, 80, "计算统计信息");
            auto stats = calculateStatistics(modulusPath);

            double meanModulus = 0.0;
            if (stats.find("mean") != stats.end())
            {
                meanModulus = stats["mean"];
            }

            updateTaskProgress(taskId, 90, "保存结果");
            models::ErosionResult erosion;
            erosion.modulus_raster_path = modulusPath;
            erosion.class_raster_path = classPath;
            erosion.mean_modulus = meanModulus;
            erosion.period_id = params.period_id;
            erosion.created_at = std::chrono::system_clock::now();

            int erosionId = erosionDao_->insert(erosion);
            erosion.id = erosionId;

            result.success = true;
            result.message = "USLE侵蚀计算完成";
            result.modulus_raster_path = modulusPath;
            result.class_raster_path = classPath;
            result.mean_modulus = meanModulus;
            result.class_areas = stats;

            updateTaskProgress(taskId, 100, "USLE侵蚀计算完成");

            utils::Logger::info("Erosion calculation completed: period={}, mean_modulus={}", params.period_id,
                                meanModulus);
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.message = std::string("USLE侵蚀计算失败: ") + e.what();
            updateTaskProgress(taskId, -1, result.message);
        }

        return result;
    }

    namespace
    {
        // K factor lookup table (11 soil texture types)
        const std::map<int, double> SOIL_K_VALUES = {{1, 0.10}, {2, 0.15}, {3, 0.25}, {4, 0.35},  {5, 0.40}, {6, 0.45},
                                                     {7, 0.35}, {8, 0.30}, {9, 0.25}, {10, 0.20}, {11, 0.15}};

        // P factor lookup table (6 conservation measure types)
        const std::map<int, double> P_FACTOR_VALUES = {{0, 1.0}, {1, 0.6}, {2, 0.5}, {3, 0.3}, {4, 0.25}, {5, 0.1}};

        // Slope-dependent exponent m for LS factor
        double lsExponent(double slopeDeg)
        {
            if (slopeDeg < 1.0)
                return 0.2;
            if (slopeDeg < 3.0)
                return 0.3;
            if (slopeDeg < 5.0)
                return 0.4;
            return 0.5;
        }

        // Cai Chongfa C factor formula
        double cFactorFromFvc(double fvc)
        {
            if (fvc <= 0.0)
                return 1.0;
            if (fvc >= 0.783)
                return 0.0;
            return std::max(0.0, std::min(1.0, 0.6508 - 0.3436 * std::log10(fvc * 100.0 + 1e-6)));
        }
    }  // namespace

    std::string ErosionService::calculateRFactor(const std::string& rainfallPath, const std::string& outputPath)
    {
        auto rainfallData = gdal_->readRaster(rainfallPath);

        std::vector<double> rData(rainfallData.data.size());
        for (size_t i = 0; i < rainfallData.data.size(); ++i)
        {
            double p = rainfallData.data[i];
            rData[i] = 0.0483 * std::pow(p, 1.610);
        }

        infrastructure::gdal::RasterData outputData;
        outputData.data = rData;
        outputData.width = rainfallData.width;
        outputData.height = rainfallData.height;
        outputData.geotransform = rainfallData.geotransform;
        outputData.projection = rainfallData.projection;

        gdal_->writeRaster(outputPath, outputData);
        return outputPath;
    }

    std::string ErosionService::calculateKFactor(const std::string& soilPath, const std::string& outputPath)
    {
        if (checkSagaAvailable())
        {
            saga_->calculateKFactor(soilPath, outputPath);
        }
        else
        {
            auto soilData = gdal_->readRaster(soilPath);

            std::vector<double> kData(soilData.data.size(), 0.30);
            for (size_t i = 0; i < soilData.data.size(); ++i)
            {
                int textureCode = static_cast<int>(std::round(soilData.data[i]));
                if (SOIL_K_VALUES.count(textureCode))
                {
                    kData[i] = SOIL_K_VALUES.at(textureCode);
                }
            }

            infrastructure::gdal::RasterData outputData;
            outputData.data = kData;
            outputData.width = soilData.width;
            outputData.height = soilData.height;
            outputData.geotransform = soilData.geotransform;
            outputData.projection = soilData.projection;

            gdal_->writeRaster(outputPath, outputData);
        }

        return outputPath;
    }

    std::string ErosionService::calculateLSFactor(const std::string& demPath, const std::string& outputPath,
                                                  const std::string& method)
    {
        if (method == "grass" && checkGrassAvailable())
        {
            grass_->calculateLSFactor(demPath, outputPath);
        }
        else if (checkSagaAvailable())
        {
            saga_->calculateLSFactor(demPath, outputPath);
        }
        else
        {
            auto demData = gdal_->readRaster(demPath);

            std::vector<double> slopeDeg(demData.data.size());
            double dx = std::abs(demData.geotransform[1]);
            double dy = std::abs(demData.geotransform[5]);
            int width = demData.width;
            int height = demData.height;
            double flowLength = 30.0;

            for (int row = 0; row < height; ++row)
            {
                for (int col = 0; col < width; ++col)
                {
                    int idx = row * width + col;
                    double dzdx = 0.0, dzdy = 0.0;

                    if (col > 0 && col < width - 1)
                    {
                        dzdx = (demData.data[idx + 1] - demData.data[idx - 1]) / (2.0 * dx);
                    }
                    if (row > 0 && row < height - 1)
                    {
                        dzdy = (demData.data[(row + 1) * width + col] - demData.data[(row - 1) * width + col]) /
                               (2.0 * dy);
                    }

                    slopeDeg[idx] = std::atan(std::sqrt(dzdx * dzdx + dzdy * dzdy)) * 180.0 / M_PI;
                }
            }

            std::vector<double> lsData(demData.data.size());
            for (size_t i = 0; i < demData.data.size(); ++i)
            {
                double slope = slopeDeg[i];
                double slopeRad = slope * M_PI / 180.0;
                double sinTheta = std::sin(slopeRad);

                double m = lsExponent(slope);
                double L = std::pow(flowLength / 22.13, m);
                double S = 65.41 * sinTheta * sinTheta + 4.56 * sinTheta + 0.065;
                lsData[i] = L * S;
            }

            infrastructure::gdal::RasterData outputData;
            outputData.data = lsData;
            outputData.width = demData.width;
            outputData.height = demData.height;
            outputData.geotransform = demData.geotransform;
            outputData.projection = demData.projection;

            gdal_->writeRaster(outputPath, outputData);
        }

        return outputPath;
    }

    std::string ErosionService::calculateCFactor(const std::string& fvcPath, const std::string& outputPath)
    {
        auto fvcData = gdal_->readRaster(fvcPath);

        std::vector<double> cData(fvcData.data.size());
        for (size_t i = 0; i < fvcData.data.size(); ++i)
        {
            cData[i] = cFactorFromFvc(fvcData.data[i]);
        }

        infrastructure::gdal::RasterData outputData;
        outputData.data = cData;
        outputData.width = fvcData.width;
        outputData.height = fvcData.height;
        outputData.geotransform = fvcData.geotransform;
        outputData.projection = fvcData.projection;

        gdal_->writeRaster(outputPath, outputData);
        return outputPath;
    }

    std::string ErosionService::calculatePFactor(const std::string& outputPath, double pValue)
    {
        infrastructure::gdal::RasterData outputData;
        outputData.width = 100;
        outputData.height = 100;
        outputData.data.resize(100 * 100, pValue);
        outputData.geotransform = {0.0, 1.0, 0.0, 0.0, 0.0, -1.0};
        outputData.projection = "";

        gdal_->writeRaster(outputPath, outputData);
        return outputPath;
    }

    std::string ErosionService::calculateUSLE(const std::string& rPath, const std::string& kPath,
                                              const std::string& lsPath, const std::string& cPath,
                                              const std::string& pPath, const std::string& outputPath)
    {
        auto rData = gdal_->readRaster(rPath);
        auto kData = gdal_->readRaster(kPath);
        auto lsData = gdal_->readRaster(lsPath);
        auto cData = gdal_->readRaster(cPath);
        auto pData = gdal_->readRaster(pPath);

        std::vector<double> usleData(rData.data.size());
        for (size_t i = 0; i < rData.data.size(); ++i)
        {
            usleData[i] = rData.data[i] * kData.data[i] * lsData.data[i] * cData.data[i] * pData.data[i];
        }

        infrastructure::gdal::RasterData outputData;
        outputData.data = usleData;
        outputData.width = rData.width;
        outputData.height = rData.height;
        outputData.geotransform = rData.geotransform;
        outputData.projection = rData.projection;

        gdal_->writeRaster(outputPath, outputData);
        return outputPath;
    }

    std::string ErosionService::classifyErosion(const std::string& modulusPath, const std::string& outputPath)
    {
        auto modulusData = gdal_->readRaster(modulusPath);

        std::vector<double> classData(modulusData.data.size());
        for (size_t i = 0; i < modulusData.data.size(); ++i)
        {
            double modulus = modulusData.data[i];

            if (modulus < 1000)
                classData[i] = 1;
            else if (modulus < 2500)
                classData[i] = 2;
            else if (modulus < 5000)
                classData[i] = 3;
            else if (modulus < 8000)
                classData[i] = 4;
            else if (modulus < 15000)
                classData[i] = 5;
            else
                classData[i] = 6;
        }

        infrastructure::gdal::RasterData outputData;
        outputData.data = classData;
        outputData.width = modulusData.width;
        outputData.height = modulusData.height;
        outputData.geotransform = modulusData.geotransform;
        outputData.projection = modulusData.projection;

        gdal_->writeRaster(outputPath, outputData);
        return outputPath;
    }

    std::map<std::string, double> ErosionService::calculateStatistics(const std::string& erosionPath)
    {
        std::map<std::string, double> stats;

        try
        {
            auto erosionData = gdal_->readRaster(erosionPath);

            double sum = 0.0;
            int count = 0;

            for (double value : erosionData.data)
            {
                if (!std::isnan(value) && !std::isinf(value))
                {
                    sum += value;
                    count++;
                }
            }

            if (count > 0)
            {
                stats["mean"] = sum / count;
            }

            stats["total_pixels"] = static_cast<double>(count);
        }
        catch (const std::exception& e)
        {
            utils::Logger::warn("Erosion statistics calculation failed: {}", e.what());
        }

        return stats;
    }

    bool ErosionService::checkGrassAvailable()
    {
        return grass_ && grass_->isAvailable();
    }

    bool ErosionService::checkSagaAvailable()
    {
        return saga_ && saga_->isAvailable();
    }

    void ErosionService::updateTaskProgress(int taskId, int progress, const std::string& message)
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
