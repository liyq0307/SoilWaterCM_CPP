#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../dao/basemap_dao.h"
#include "../infrastructure/gdal/gdal_wrapper.h"
#include "../models/basemap.h"

namespace soilwatercm::services
{

    /** @brief 上传底图入参 */
    struct BasemapUploadParams
    {
        int project_id = 0;                 ///< 归属项目 id
        std::string name;                   ///< 底图名称 (空则用原始文件名)
        std::string note;                   ///< 备注 (可空)
        std::string temp_file_path;         ///< 已落盘的临时文件完整路径
        std::string original_filename;      ///< 上传原始文件名
    };

    /**
     * @brief 项目底图管理服务
     *
     * 上传影像底图, 用 GDAL 读元数据并计算拉伸参数与覆盖范围,
     * 入库后供 QGIS Server 以 WMS 图层叠加显示。
     */
    class BasemapService
    {
    public:
        /**
         * @brief 构造, 注入底图 DAO 与 GDAL 封装
         * @param basemapDao 底图 DAO
         * @param gdal GDAL 封装 (读栅格元数据)
         */
        BasemapService(std::shared_ptr<dao::BasemapDao> basemapDao,
                       std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal);

        /**
         * @brief 列出项目底图
         * @param projectId 项目 id
         * @return 底图记录列表
         */
        std::vector<models::BasemapImage> list(int projectId);

        /**
         * @brief 上传底图
         *
         * 保存到存储目录, GDAL 读取栅格元数据 (尺寸/波段/范围/CRS),
         * 计算 2%/98% 百分位拉伸与 footprint, 入库并生成 wms_layer_name。
         * @param params 上传入参
         * @return 含新 id 的底图记录
         * @throw ValidationException 清洗后文件名为空时
         * @throw FileNotFoundException 临时文件不存在时
         * @throw FileException 拷贝到存储目录失败时
         * @throw DatabaseException 底图记录入库失败时
         */
        models::BasemapImage upload(const BasemapUploadParams& params);

        /**
         * @brief 删除底图
         * @param id 底图主键
         * @return 删除成功返回 true; 物理文件及附属文件尽力删除
         */
        bool remove(int id);

        /**
         * @brief 按 id 查底图记录
         * @param id 底图主键
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::BasemapImage> getById(int id);

    private:
        std::shared_ptr<dao::BasemapDao> basemapDao_;
        std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal_;
    };

}  // namespace soilwatercm::services
