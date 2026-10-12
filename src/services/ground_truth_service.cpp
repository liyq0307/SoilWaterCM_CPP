#include "ground_truth_service.h"

#include <chrono>
#include <cmath>

#include "../common/exceptions.h"

namespace soilwatercm::services
{

    GroundTruthService::GroundTruthService(std::shared_ptr<dao::GroundTruthDao> groundTruthDao, double errorThreshold)
        : groundTruthDao_(std::move(groundTruthDao)), errorThreshold_(errorThreshold)
    {
    }

    std::vector<models::GroundTruth> GroundTruthService::list(std::optional<int> projectId, std::optional<int> periodId,
                                                              const std::string& sampleType, int page, int pageSize)
    {
        return groundTruthDao_->list(projectId, periodId, sampleType, page, pageSize);
    }

    models::GroundTruth GroundTruthService::create(const models::GroundTruth& gt)
    {
        if (gt.project_id <= 0)
            throw common::ValidationException("project_id 无效");
        if (gt.sample_type.empty())
            throw common::ValidationException("sample_type 不能为空");
        if (gt.location.wkt.empty())
            throw common::ValidationException("location 无效, 需要有效的 POINT 几何");

        models::GroundTruth rec = gt;
        rec.created_at = std::chrono::system_clock::now();
        int id = groundTruthDao_->insert(rec);
        if (id < 0)
            throw common::DatabaseException("创建地面验证记录失败");
        rec.id = id;
        return rec;
    }

    nlohmann::json GroundTruthService::verifyAccuracy(int id)
    {
        auto existing = groundTruthDao_->findById(id);
        if (!existing)
            throw common::NotFoundException("验证点不存在");

        models::GroundTruth gt = *existing;
        nlohmann::json result;
        result["id"] = gt.id;

        // 优先从关联的遥感目标表读取提取值, 否则回退到记录中已存的 rs_value
        std::optional<double> rs;
        if (!gt.target_table.empty() && gt.target_id.has_value())
            rs = groundTruthDao_->fetchRsValue(gt.target_table, *gt.target_id);
        const bool hasTarget = !gt.target_table.empty() && gt.target_id.has_value();
        if (!rs.has_value())
            rs = gt.rs_value;

        if (!rs.has_value())
        {
            result["status"] = hasTarget ? "缺少对照值" : "未关联遥感目标";
            result["passed"] = nullptr;
            return result;
        }

        if (!gt.measured_value.has_value())
        {
            gt.rs_value = rs;
            groundTruthDao_->update(gt);
            result["status"] = "缺少对照值";
            result["passed"] = nullptr;
            return result;
        }

        // 相对误差 = |rs - measured| / (measured + 1e-9), 与 Python accuracy_service 公式一致
        double error = std::abs(*rs - *gt.measured_value) / (*gt.measured_value + 1e-9);
        bool passed = error <= errorThreshold_;

        gt.rs_value = rs;
        gt.error = error;
        gt.accuracy_pass = passed;
        groundTruthDao_->update(gt);

        result["measured"] = *gt.measured_value;
        result["rs_value"] = *rs;
        result["error"] = error;
        result["passed"] = passed;
        result["threshold"] = errorThreshold_;
        return result;
    }

}  // namespace soilwatercm::services
