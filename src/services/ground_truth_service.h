#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../dao/ground_truth_dao.h"
#include "../models/ground_truth.h"
#include <nlohmann/json.hpp>

namespace soilwatercm::services
{

    /**
     * @brief 地面验证点管理服务
     *
     * 记录实测样本值, 与遥感提取值对照算相对误差,
     * 判定提取精度是否达标 (误差阈值可配)。
     */
    class GroundTruthService
    {
    public:
        /**
         * @brief 构造, 注入验证点 DAO
         * @param groundTruthDao 地面验证 DAO
         * @param errorThreshold 相对误差达标阈值 (默认 0.10, 参考 SL/T 277-2024 扰动误差限)
         */
        explicit GroundTruthService(std::shared_ptr<dao::GroundTruthDao> groundTruthDao, double errorThreshold = 0.10);

        /**
         * @brief 分页查验证点
         * @param projectId 项目 id; 为空则不过滤
         * @param periodId 期次 id; 为空则不过滤
         * @param sampleType 样本类型; 为空则不过滤
         * @param page 页码, 从 1 开始
         * @param pageSize 每页条数
         * @return 验证点列表
         */
        std::vector<models::GroundTruth> list(std::optional<int> projectId, std::optional<int> periodId,
                                              const std::string& sampleType, int page = 1, int pageSize = 50);

        /**
         * @brief 创建验证点
         * @param gt 待插入记录
         * @return 含新 id 的记录
         * @throw ValidationException project_id 无效 / sample_type 为空 /
         *        location 非 POINT 几何时
         * @throw DatabaseException 入库失败时
         */
        models::GroundTruth create(const models::GroundTruth& gt);

        /**
         * @brief 精度验证
         *
         * 优先从关联遥感目标表取提取值 (无关联则用记录中已存的 rs_value),
         * 计算相对误差并回写。
         * @param id 验证点主键
         * @return 验证结果 JSON (status/passed/relative_error 等)
         * @throw NotFoundException 验证点不存在时
         */
        nlohmann::json verifyAccuracy(int id);

    private:
        std::shared_ptr<dao::GroundTruthDao> groundTruthDao_;
        double errorThreshold_;
    };

}  // namespace soilwatercm::services
