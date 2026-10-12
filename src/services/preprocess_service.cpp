#include "preprocess_service.h"

#include <filesystem>

#include "../common/exceptions.h"
#include "../utils/logger.h"

namespace soilwatercm::services
{

    PreprocessService::PreprocessService(std::shared_ptr<dao::ImageDao> imageDao, std::shared_ptr<dao::TaskDao> taskDao,
                                         std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal,
                                         std::shared_ptr<infrastructure::gdal::RasterProcessor> rasterProcessor,
                                         std::shared_ptr<infrastructure::OtbInterface> otb)
        : imageDao_(std::move(imageDao)),
          taskDao_(std::move(taskDao)),
          gdal_(std::move(gdal)),
          rasterProcessor_(std::move(rasterProcessor)),
          otb_(std::move(otb))
    {
    }

    std::future<PreprocessResult> PreprocessService::submitPreprocessTask(const PreprocessParams& params)
    {
        return std::async(std::launch::async,
                          [this, params]()
                          {
                              models::Task task;
                              task.type = models::TaskType::Preprocess;
                              task.status = models::TaskStatus::Pending;
                              task.submitted_at = std::chrono::system_clock::now();
                              task.project_id = 0;

                              int taskId = taskDao_->insert(task);

                              return this->executePreprocess(params, taskId);
                          });
    }

    PreprocessResult PreprocessService::executePreprocess(const PreprocessParams& params, int taskId)
    {
        PreprocessResult result;
        result.task_id = taskId;

        try
        {
            updateTaskProgress(taskId, 0, "开始预处理");

            std::vector<std::string> inputPaths;
            for (int imageId : params.image_ids)
            {
                auto imageOpt = imageDao_->findById(imageId);
                if (!imageOpt)
                {
                    throw common::NotFoundException("影像不存在: " + std::to_string(imageId));
                }
                inputPaths.push_back(imageOpt->file_path);
            }

            std::filesystem::create_directories(params.output_dir);

            std::vector<std::string> outputPaths;
            int totalSteps = static_cast<int>(inputPaths.size());
            int currentStep = 0;

            for (const auto& inputPath : inputPaths)
            {
                std::string currentPath = inputPath;

                if (params.radiometric_correction)
                {
                    std::string outputPath = generateOutputPath(currentPath, "_rad");
                    auto radResult = radiometricCorrection(currentPath, outputPath);
                    if (!radResult.success)
                    {
                        throw common::ProcessingException(radResult.message);
                    }
                    currentPath = outputPath;
                }

                if (params.geometric_correction && params.target_epsg)
                {
                    std::string outputPath = generateOutputPath(currentPath, "_geom");
                    auto geomResult = geometricCorrection(currentPath, outputPath, *params.target_epsg);
                    if (!geomResult.success)
                    {
                        throw common::ProcessingException(geomResult.message);
                    }
                    currentPath = outputPath;
                }

                if (params.orthorectification)
                {
                    std::string outputPath = generateOutputPath(currentPath, "_ortho");
                    auto orthoResult = orthorectification(currentPath, outputPath);
                    if (!orthoResult.success)
                    {
                        throw common::ProcessingException(orthoResult.message);
                    }
                    currentPath = outputPath;
                }

                if (params.target_resolution)
                {
                    std::string outputPath = generateOutputPath(currentPath, "_resamp");
                    auto resampResult = resample(currentPath, outputPath, *params.target_resolution);
                    if (!resampResult.success)
                    {
                        throw common::ProcessingException(resampResult.message);
                    }
                    currentPath = outputPath;
                }

                outputPaths.push_back(currentPath);

                currentStep++;
                int progress = static_cast<int>((currentStep * 100.0) / totalSteps);
                updateTaskProgress(taskId, progress,
                                   "处理影像 " + std::to_string(currentStep) + "/" + std::to_string(totalSteps));
            }

            result.success = true;
            result.message = "预处理完成";
            result.output_files = outputPaths;

            updateTaskProgress(taskId, 100, "预处理完成");
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.message = std::string("预处理失败: ") + e.what();
            updateTaskProgress(taskId, -1, result.message);
        }

        return result;
    }

    std::future<PreprocessResult> PreprocessService::submitMosaicTask(const MosaicParams& params)
    {
        return std::async(std::launch::async,
                          [this, params]()
                          {
                              models::Task task;
                              task.type = models::TaskType::Preprocess;
                              task.status = models::TaskStatus::Pending;
                              task.submitted_at = std::chrono::system_clock::now();
                              task.project_id = 0;

                              int taskId = taskDao_->insert(task);

                              return this->executeMosaic(params, taskId);
                          });
    }

    PreprocessResult PreprocessService::executeMosaic(const MosaicParams& params, int taskId)
    {
        PreprocessResult result;
        result.task_id = taskId;

        try
        {
            updateTaskProgress(taskId, 0, "开始镶嵌");

            std::vector<std::string> inputPaths;
            for (int imageId : params.image_ids)
            {
                auto imageOpt = imageDao_->findById(imageId);
                if (!imageOpt)
                {
                    throw common::NotFoundException("影像不存在: " + std::to_string(imageId));
                }
                inputPaths.push_back(imageOpt->file_path);
            }

            updateTaskProgress(taskId, 50, "执行镶嵌");

            rasterProcessor_->mosaic(inputPaths, params.output_path, params.method, params.nodata_value);

            result.success = true;
            result.message = "镶嵌完成";
            result.output_files.push_back(params.output_path);

            updateTaskProgress(taskId, 100, "镶嵌完成");
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.message = std::string("镶嵌失败: ") + e.what();
            updateTaskProgress(taskId, -1, result.message);
        }

        return result;
    }

    PreprocessResult PreprocessService::radiometricCorrection(const std::string& inputPath,
                                                              const std::string& outputPath)
    {
        PreprocessResult result;

        try
        {
            if (checkOtbAvailable())
            {
                otb_->opticalCalibration(inputPath, outputPath);
                result.success = true;
                result.message = "辐射校正完成 (OTB)";
            }
            else
            {
                utils::Logger::warn("OTB不可用，跳过辐射校正");
                std::filesystem::copy_file(inputPath, outputPath, std::filesystem::copy_options::overwrite_existing);
                result.success = true;
                result.message = "辐射校正跳过 (OTB不可用)";
            }

            result.output_files.push_back(outputPath);
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.message = std::string("辐射校正失败: ") + e.what();
        }

        return result;
    }

    PreprocessResult PreprocessService::geometricCorrection(const std::string& inputPath, const std::string& outputPath,
                                                            int targetEpsg)
    {
        PreprocessResult result;

        try
        {
            gdal_->reproject(inputPath, outputPath, targetEpsg);

            result.success = true;
            result.message = "几何校正完成";
            result.output_files.push_back(outputPath);
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.message = std::string("几何校正失败: ") + e.what();
        }

        return result;
    }

    PreprocessResult PreprocessService::orthorectification(const std::string& inputPath, const std::string& outputPath,
                                                           const std::optional<std::string>& demPath)
    {
        PreprocessResult result;

        try
        {
            if (checkOtbAvailable())
            {
                otb_->orthoRectification(inputPath, outputPath, demPath.value_or(""));
                result.success = true;
                result.message = "正射纠正完成 (OTB)";
            }
            else
            {
                utils::Logger::warn("OTB不可用，使用GDAL重投影代替正射纠正");
                gdal_->reproject(inputPath, outputPath, 4326);
                result.success = true;
                result.message = "正射纠正完成 (GDAL)";
            }

            result.output_files.push_back(outputPath);
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.message = std::string("正射纠正失败: ") + e.what();
        }

        return result;
    }

    PreprocessResult PreprocessService::resample(const std::string& inputPath, const std::string& outputPath,
                                                 double targetResolution)
    {
        PreprocessResult result;

        try
        {
            rasterProcessor_->resample(inputPath, outputPath, targetResolution, "bilinear");

            result.success = true;
            result.message = "重采样完成";
            result.output_files.push_back(outputPath);
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.message = std::string("重采样失败: ") + e.what();
        }

        return result;
    }

    bool PreprocessService::checkOtbAvailable()
    {
        return otb_ && otb_->isAvailable();
    }

    bool PreprocessService::checkGdalAvailable()
    {
        return gdal_ != nullptr;
    }

    std::string PreprocessService::generateOutputPath(const std::string& inputPath, const std::string& suffix)
    {
        std::filesystem::path p(inputPath);
        std::string stem = p.stem().string();
        std::string ext = p.extension().string();
        std::filesystem::path parent = p.parent_path();

        return (parent / (stem + suffix + ext)).string();
    }

    void PreprocessService::updateTaskProgress(int taskId, int progress, const std::string& message)
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

}  // namespace soilwatercm::services
