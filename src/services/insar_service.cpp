#include "insar_service.h"

#include <filesystem>

#include "../common/exceptions.h"
#include "../infrastructure/gdal/gdal_wrapper.h"
#include "../utils/logger.h"

namespace soilwatercm::services
{

    InsarService::InsarService(std::shared_ptr<dao::TaskDao> taskDao,
                               std::shared_ptr<infrastructure::SnaphuWrapper> snaphu,
                               std::shared_ptr<infrastructure::InterferogramGenerator> interferogram,
                               std::shared_ptr<infrastructure::PhaseFilter> phaseFilter,
                               std::shared_ptr<infrastructure::DeformationCalculator> deformationCalc,
                               std::shared_ptr<infrastructure::MintPyInterface> mintpy,
                               std::shared_ptr<infrastructure::Pyaps3Interface> pyaps3)
        : taskDao_(std::move(taskDao)),
          snaphu_(std::move(snaphu)),
          interferogram_(std::move(interferogram)),
          phaseFilter_(std::move(phaseFilter)),
          deformationCalc_(std::move(deformationCalc)),
          mintpy_(std::move(mintpy)),
          pyaps3_(std::move(pyaps3))
    {
    }

    std::future<InsarProcessResult> InsarService::submitInsarTask(const InsarProcessParams& params)
    {
        models::Task task;
        task.type = models::TaskType::Insar;
        task.status = models::TaskStatus::Pending;
        task.submitted_at = std::chrono::system_clock::now();
        task.project_id = params.project_id;
        task.period_id = params.period_id;

        int taskId = taskDao_->insert(task);

        return std::async(std::launch::async, [this, params, taskId]()
                          {
                              if (params.process_type == "dinsar")
                              {
                                  return this->executeDInsar(params, taskId);
                              }
                              else
                              {
                                  return this->executeSbas(params, taskId);
                              }
                          });
    }

    std::future<InsarProcessResult> InsarService::submitInsarTask(const InsarProcessParams& params, int taskId)
    {
        if (taskId > 0)
        {
            updateTaskProgress(taskId, 0, "任务已登记");
        }
        return std::async(std::launch::async, [this, params, taskId]()
                          {
                              if (params.process_type == "dinsar")
                              {
                                  return this->executeDInsar(params, taskId);
                              }
                              else
                              {
                                  return this->executeSbas(params, taskId);
                              }
                          });
    }

    InsarProcessResult InsarService::executeDInsar(const InsarProcessParams& params, int taskId)
    {
        InsarProcessResult result;
        result.task_id = taskId;

        try
        {
            updateTaskProgress(taskId, 0, "开始D-InSAR处理");

            std::filesystem::create_directories(params.output_dir);

            updateTaskProgress(taskId, 10, "生成干涉图");
            std::string ifgPath = (std::filesystem::path(params.output_dir) / "interferogram.tif").string();
            generateInterferogram(params.master_slc_path, params.slave_slc_path, ifgPath);
            result.interferogram_path = ifgPath;

            updateTaskProgress(taskId, 30, "相位滤波");
            std::string filteredPath = (std::filesystem::path(params.output_dir) / "filtered_phase.tif").string();
            filterPhase(ifgPath, filteredPath, "goldstein");
            result.filtered_phase_path = filteredPath;

            updateTaskProgress(taskId, 50, "相位解缠");
            std::string unwrappedPath = (std::filesystem::path(params.output_dir) / "unwrapped_phase.tif").string();
            std::string coherencePath = filteredPath;
            unwrapPhase(filteredPath, coherencePath, unwrappedPath);
            result.unwrapped_phase_path = unwrappedPath;

            updateTaskProgress(taskId, 70, "形变计算");
            std::string deformationPath = (std::filesystem::path(params.output_dir) / "deformation.tif").string();
            calculateDeformation(unwrappedPath, deformationPath, 0.056);
            result.deformation_path = deformationPath;

            updateTaskProgress(taskId, 90, "大气校正");
            std::string correctedPath =
                (std::filesystem::path(params.output_dir) / "deformation_corrected.tif").string();
            atmosphericCorrection(deformationPath, correctedPath);

            result.success = true;
            result.message = "D-InSAR处理完成";

            updateTaskProgress(taskId, 100, "D-InSAR处理完成");

            utils::Logger::info("D-InSAR processing completed: project={}", params.project_id);
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.message = std::string("D-InSAR处理失败: ") + e.what();
            updateTaskProgress(taskId, -1, result.message);
        }

        return result;
    }

    InsarProcessResult InsarService::executeSbas(const InsarProcessParams& params, int taskId)
    {
        InsarProcessResult result;
        result.task_id = taskId;

        try
        {
            updateTaskProgress(taskId, 0, "开始SBAS时序分析");

            if (checkMintpyAvailable())
            {
                updateTaskProgress(taskId, 50, "执行MintPy时序分析");

                std::string configPath = (std::filesystem::path(params.output_dir) / "mintpy_config.txt").string();
                mintpy_->generateConfig(params.output_dir, configPath);

                mintpy_->runSbas(configPath, params.output_dir);

                result.success = true;
                result.message = "SBAS时序分析完成";
            }
            else
            {
                result.success = false;
                result.message = "MintPy不可用，无法执行SBAS时序分析";
            }

            updateTaskProgress(taskId, 100, "SBAS时序分析完成");
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.message = std::string("SBAS时序分析失败: ") + e.what();
            updateTaskProgress(taskId, -1, result.message);
        }

        return result;
    }

    std::string InsarService::generateInterferogram(const std::string& masterPath, const std::string& slavePath,
                                                    const std::string& outputPath)
    {
        interferogram_->generateFromFiles(masterPath, slavePath, outputPath);
        return outputPath;
    }

    std::string InsarService::filterPhase(const std::string& ifgPath, const std::string& outputPath,
                                          const std::string& method)
    {
        infrastructure::gdal::GdalWrapper gdal;
        auto phaseData = gdal.readRaster(ifgPath);
        auto coherenceData = gdal.readRaster(ifgPath);
        infrastructure::FilterParams filterParams;
        if (method == "goldstein")
        {
            filterParams.method = infrastructure::FilterMethod::Goldstein;
        }
        auto result = phaseFilter_->filter(phaseData, coherenceData, filterParams);
        gdal.writeRaster(outputPath, result);
        return outputPath;
    }

    std::string InsarService::unwrapPhase(const std::string& phasePath, const std::string& coherencePath,
                                          const std::string& outputPath)
    {
        snaphu_->unwrapFile(phasePath, coherencePath, outputPath, infrastructure::SnaphuParams{});
        return outputPath;
    }

    std::string InsarService::calculateDeformation(const std::string& unwrappedPath, const std::string& outputPath,
                                                   double wavelength)
    {
        infrastructure::gdal::GdalWrapper gdal;
        auto phaseData = gdal.readRaster(unwrappedPath);
        infrastructure::DeformationParams deformParams;
        deformParams.wavelength = wavelength;
        auto result = deformationCalc_->calculate(phaseData, deformParams);
        gdal.writeRaster(outputPath, result.losDeformation);
        return outputPath;
    }

    std::string InsarService::atmosphericCorrection(const std::string& deformationPath, const std::string& outputPath)
    {
        if (pyaps3_ && pyaps3_->isAvailable())
        {
            pyaps3_->correctAtmosphere(deformationPath, outputPath);
        }
        else
        {
            utils::Logger::warn("PyAPS3不可用，跳过大气校正");
            return deformationPath;
        }

        return outputPath;
    }

    bool InsarService::checkSnaphuAvailable()
    {
        return snaphu_ != nullptr;
    }

    bool InsarService::checkMintpyAvailable()
    {
        return mintpy_ && mintpy_->isAvailable();
    }

    void InsarService::updateTaskProgress(int taskId, int progress, const std::string& message)
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
