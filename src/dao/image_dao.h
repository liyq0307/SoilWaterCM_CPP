#pragma once

#include "../models/image.h"
#include "dao_base.h"

namespace soilwatercm::dao
{

    /**
     * @brief 遥感影像数据访问对象
     *
     * 对应 images 表: 遥感影像元数据 (编码、传感器、拍摄时间、范围几何、
     * 文件路径、质量状态)。
     */
    class ImageDao : public DaoBase<models::Image>
    {
    public:
        /**
         * @brief 构造, 注入数据库仓库
         * @param repo PostgreSQL 访问仓库
         */
        explicit ImageDao(std::shared_ptr<repository::PostgresRepository> repo);

        /**
         * @brief 按 id 查影像
         * @param id 影像主键
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::Image> findById(int id) override;

        /**
         * @brief 按项目查影像
         * @param projectId 项目 id
         * @return 匹配记录列表
         */
        virtual std::vector<models::Image> findByProjectId(int projectId);

        /**
         * @brief 按期次查影像
         * @param periodId 期次 id
         * @return 匹配记录列表
         */
        virtual std::vector<models::Image> findByPeriodId(int periodId);

        /**
         * @brief 按项目和期次查影像
         * @param projectId 项目 id
         * @param periodId 期次 id
         * @return 匹配记录列表
         */
        std::vector<models::Image> findByProjectAndPeriod(int projectId, int periodId);

        /**
         * @brief 按质量状态查影像
         * @param status 质量状态
         * @return 匹配记录列表
         */
        std::vector<models::Image> findByQualityStatus(models::QualityStatus status);

        /**
         * @brief 取项目最近 N 期 SAR 影像 (按采集时间倒序)
         *
         * 供 InSAR 选主/从影像: 取 2 期时较老为主影像, 较新为从影像。
         * @param projectId 项目 id
         * @param limit 返回条数上限
         * @return 按采集时间倒序的 SAR 影像列表
         */
        std::vector<models::Image> findLatestSar(int projectId, int limit = 2);

        /**
         * @brief 分页列出所有影像
         * @param page 页码, 从 1 开始
         * @param pageSize 每页条数
         * @param orderBy 排序列名
         * @param ascending 是否升序
         * @return 当前页记录列表
         */
        std::vector<models::Image> list(int page = 1, int pageSize = 20, const std::string& orderBy = "id",
                                        bool ascending = true) override;

        /**
         * @brief 新增影像
         * @param image 待插入记录; 范围以 ST_GeomFromText 写入 (SRID 4326)
         * @return 新记录 id; 失败返回 -1
         */
        int insert(const models::Image& image) override;

        /**
         * @brief 更新影像质量状态 (可选消息)
         * @param image 记录 (须含 id); quality_message 为空则不更新该列
         * @return 更新成功返回 true
         */
        bool update(const models::Image& image) override;

        /**
         * @brief 按 id 删除影像
         * @param id 影像主键
         * @return 删除成功返回 true
         */
        bool remove(int id) override;

        /**
         * @brief 影像总数
         * @return 表内记录数
         */
        int count() override;

        /**
         * @brief 更新质量状态并写入检查消息
         * @param id 影像主键
         * @param status 新质量状态
         * @param message 检查消息
         * @return 更新成功返回 true
         */
        virtual bool updateQualityStatus(int id, models::QualityStatus status, const std::string& message);

    private:
        /// @brief 行数据 -> 实体 (范围以 WKT 读出)
        models::Image mapRowToImage(const std::map<std::string, std::string>& row);
    };

}  // namespace soilwatercm::dao
