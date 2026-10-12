#include "measure_service.h"

#include <filesystem>

#include "../common/exceptions.h"
#include "../utils/logger.h"

namespace soilwatercm::services
{

    MeasureService::MeasureService(std::shared_ptr<dao::MeasureDao> measureDao, std::shared_ptr<dao::TaskDao> taskDao,
                                   std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal,
                                   std::shared_ptr<infrastructure::gdal::VectorProcessor> vectorProcessor)
        : measureDao_(std::move(measureDao)),
          taskDao_(std::move(taskDao)),
          gdal_(std::move(gdal)),
          vectorProcessor_(std::move(vectorProcessor))
    {
    }

    std::future<MeasureResult> MeasureService::submitMeasureTask(const MeasureParams& params)
    {
        return std::async(std::launch::async,
                          [this, params]()
                          {
                              models::Task task;
                              task.type = models::TaskType::MeasureInterpretation;
                              task.status = models::TaskStatus::Pending;
                              task.submitted_at = std::chrono::system_clock::now();
                              task.period_id = params.period_id;

                              int taskId = taskDao_->insert(task);

                              return this->executeMeasure(params, taskId);
                          });
    }

    MeasureResult MeasureService::executeMeasure(const MeasureParams& params, int taskId)
    {
        MeasureResult result;
        result.task_id = taskId;

        try
        {
            updateTaskProgress(taskId, 0, "开始水保措施解译");

            std::string measureType = params.measure_type.value_or("");
            auto measures = detectMeasures(params.image_path, measureType);

            for (auto& measure : measures)
            {
                calculateMeasureAttributes(measure);
                measure.period_id = params.period_id;
                measure.created_at = std::chrono::system_clock::now();

                int id = measureDao_->insert(measure);
                measure.id = id;
            }

            result.success = true;
            result.message = "水保措施解译完成";
            result.measures = measures;
            result.total_count = static_cast<int>(measures.size());

            updateTaskProgress(taskId, 100, "水保措施解译完成");

            utils::Logger::info("Measure interpretation completed: period={}, count={}", params.period_id,
                                result.total_count);
        }
        catch (const std::exception& e)
        {
            result.success = false;
            result.message = std::string("水保措施解译失败: ") + e.what();
            updateTaskProgress(taskId, -1, result.message);
        }

        return result;
    }

    std::vector<models::Measure> MeasureService::detectMeasures(const std::string& imagePath,
                                                                const std::string& measureType)
    {
        std::vector<models::Measure> measures;

        auto geometries = vectorProcessor_->detectFeatures(imagePath);

        for (size_t i = 0; i < geometries.size(); ++i)
        {
            models::Measure measure;
            measure.type = models::MeasureType::CheckDam;
            measure.geometry = geometries[i];
            measure.status = "Active";
            measure.condition = "Good";

            measures.push_back(measure);
        }

        return measures;
    }

    void MeasureService::calculateMeasureAttributes(models::Measure& measure)
    {
        if (!measure.geometry.wkt.empty())
        {
            measure.area = gdal_->calculateArea(measure.geometry.wkt);
            measure.length = gdal_->calculateLength(measure.geometry.wkt);
        }
    }

    double MeasureService::classifyVegetationCoverage(double fvcValue)
    {
        if (fvcValue < 0.3)
            return 1;
        if (fvcValue < 0.5)
            return 2;
        if (fvcValue < 0.7)
            return 3;
        return 4;
    }

    std::vector<models::Measure> MeasureService::getMeasuresByPeriod(int periodId)
    {
        return measureDao_->findByPeriodId(periodId);
    }

    std::vector<models::Measure> MeasureService::getMeasuresByProject(int projectId, std::optional<int> periodId)
    {
        return measureDao_->findByProject(projectId, periodId);
    }

    int MeasureService::createMeasure(const models::Measure& measure)
    {
        return measureDao_->insert(measure);
    }

    void MeasureService::updateTaskProgress(int taskId, int progress, const std::string& message)
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
