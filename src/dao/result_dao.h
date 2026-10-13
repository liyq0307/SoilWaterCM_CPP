#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../models/landuse_result.h"
#include "../models/segmentation_result.h"
#include "dao_base.h"

namespace soilwatercm::dao
{
    /**
     * @brief 土地利用分类成果数据访问对象
     *
     * 对应 landuse_results 表。
     */
    class LandUseResultDao : public DaoBase<models::LandUseResult>
    {
    public:
        explicit LandUseResultDao(std::shared_ptr<repository::PostgresRepository> repo);

        std::optional<models::LandUseResult> findById(int id) override;
        std::vector<models::LandUseResult> list(int page = 1, int pageSize = 20, const std::string& orderBy = "id",
                                                 bool ascending = true) override;
        int insert(const models::LandUseResult& lu) override;
        bool update(const models::LandUseResult& lu) override;
        bool remove(int id) override;
        int count() override;

        /**
         * @brief 按项目查分类成果, 可按期次过滤
         * @param projectId 项目 id
         * @param periodId 期次 id; 为空则全期次
         * @return 按创建时间倒序的成果列表
         */
        std::vector<models::LandUseResult> findByProject(int projectId, std::optional<int> periodId = std::nullopt);

    private:
        /// @brief 行数据 -> 实体
        models::LandUseResult mapRow(const std::map<std::string, std::string>& row);
    };

    /**
     * @brief 影像分割成果数据访问对象
     *
     * 对应 segmentation_results 表。
     */
    class SegmentationResultDao : public DaoBase<models::SegmentationResult>
    {
    public:
        explicit SegmentationResultDao(std::shared_ptr<repository::PostgresRepository> repo);

        std::optional<models::SegmentationResult> findById(int id) override;
        std::vector<models::SegmentationResult> list(int page = 1, int pageSize = 20,
                                                      const std::string& orderBy = "id",
                                                      bool ascending = true) override;
        int insert(const models::SegmentationResult& seg) override;
        bool update(const models::SegmentationResult& seg) override;
        bool remove(int id) override;
        int count() override;

        /**
         * @brief 按影像/项目查分割成果
         * @param imageId 影像 id; 为空则不过滤
         * @param projectId 项目 id; 为空则不过滤
         * @return 按创建时间倒序的成果列表
         */
        std::vector<models::SegmentationResult> findByFilter(std::optional<int> imageId,
                                                             std::optional<int> projectId);

    private:
        /// @brief 行数据 -> 实体
        models::SegmentationResult mapRow(const std::map<std::string, std::string>& row);
    };
}  // namespace soilwatercm::dao
