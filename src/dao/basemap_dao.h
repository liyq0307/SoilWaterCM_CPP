#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../models/basemap.h"
#include "dao_base.h"

namespace soilwatercm::dao
{

    /**
     * @brief 底图影像数据访问对象
     *
     * 对应 basemap_images 表: 项目底图影像 (文件路径、尺寸、CRS、覆盖范围、
     * 拉伸参数、WMS 图层名)。
     */
    class BasemapDao : public DaoBase<models::BasemapImage>
    {
    public:
        /**
         * @brief 构造, 注入数据库仓库
         * @param repo PostgreSQL 访问仓库
         */
        explicit BasemapDao(std::shared_ptr<repository::PostgresRepository> repo);

        /**
         * @brief 按 id 查底图
         * @param id 底图主键
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::BasemapImage> findById(int id) override;

        /**
         * @brief 分页列出所有底图
         * @param page 页码, 从 1 开始
         * @param pageSize 每页条数
         * @param orderBy 排序列名
         * @param ascending 是否升序
         * @return 当前页记录列表
         */
        std::vector<models::BasemapImage> list(int page = 1, int pageSize = 20, const std::string& orderBy = "id",
                                               bool ascending = true) override;

        /**
         * @brief 新增底图
         * @param basemap 待插入记录; 覆盖范围以 ST_GeomFromText 写入 (SRID 4326), 为空写 NULL
         * @return 新记录 id; 失败返回 -1
         */
        int insert(const models::BasemapImage& basemap) override;

        /**
         * @brief 按 id 更新底图全部字段
         * @param basemap 记录 (须含 id); 覆盖范围以 ST_GeomFromText 写入 (SRID 4326)
         * @return 更新成功返回 true
         */
        bool update(const models::BasemapImage& basemap) override;

        /**
         * @brief 按 id 删除底图
         * @param id 底图主键
         * @return 删除成功返回 true
         */
        bool remove(int id) override;

        /**
         * @brief 底图总数
         * @return 表内记录数
         */
        int count() override;

        /**
         * @brief 按项目分页列出底图
         * @param projectId 项目 id
         * @param page 页码, 从 1 开始
         * @param pageSize 每页条数
         * @return 当前页记录, 按 id 升序
         */
        std::vector<models::BasemapImage> findByProject(int projectId, int page = 1, int pageSize = 100);

    private:
        /// @brief 行数据 -> 实体 (覆盖范围以 WKT 读出, CRS 缺失时默认 EPSG:4326)
        models::BasemapImage mapRow(const std::map<std::string, std::string>& row);
    };

}  // namespace soilwatercm::dao
