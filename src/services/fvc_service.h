#pragma once

#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../dao/image_dao.h"
#include "../dao/task_dao.h"
#include "../dao/vegetation_coverage_dao.h"
#include "../infrastructure/gdal/gdal_wrapper.h"
#include "../infrastructure/gdal/raster_processor.h"
#include "../models/vegetation_coverage.h"

namespace soilwatercm::services
{

    /** @brief FVC 反演入参 */
    struct FvcParams
    {
        int period_id;                          ///< 期次 id
        std::vector<int> image_ids;             ///< 影像 id 列表 (取首幅)
        std::string output_dir;                 ///< 输出目录
        std::optional<double> ndvi_soil = 0.05; ///< 全裸土 NDVI 端元
        std::optional<double> ndvi_veg = 0.70;  ///< 全植被 NDVI 端元
        double soil_percentile = 5.0;           ///< 裸土端元百分位
        double veg_percentile = 95.0;           ///< 植被端元百分位
    };

    /** @brief FVC 反演结果 */
    struct FvcResult
    {
        bool success = false;                          ///< 是否成功
        std::string message;                           ///< 结果说明/失败原因
        std::optional<std::string> fvc_raster_path;    ///< FVC 栅格路径
        double mean_fvc = 0.0;                         ///< 平均覆盖度
        std::map<std::string, double> class_areas;     ///< 各分级像元数/面积
        std::optional<int> task_id;                    ///< 关联任务 id
    };

    /** @brief NDVI 计算结果 */
    struct NdviResult
    {
        bool success = false;                        ///< 是否成功
        std::string message;                         ///< 结果说明/失败原因
        std::optional<std::string> ndvi_raster_path; ///< NDVI 栅格路径
    };

    /**
     * @brief 植被覆盖度反演服务
     *
     * 先由红/近红外波段算 NDVI, 再用像元二分模型
     * FVC=(NDVI-NDVIsoil)/(NDVIveg-NDVIsoil) 换算覆盖度, 统计分级后入库。
     */
    class FvcService
    {
    public:
        /**
         * @brief 构造, 注入 FVC/影像/任务 DAO 与 GDAL、栅格处理器
         * @param fvcDao 植被覆盖度 DAO (入库)
         * @param imageDao 影像 DAO (取输入文件)
         * @param taskDao 任务 DAO (进度回写)
         * @param gdal GDAL 封装
         * @param rasterProcessor 栅格处理器 (波段运算)
         */
        explicit FvcService(std::shared_ptr<dao::VegetationCoverageDao> fvcDao, std::shared_ptr<dao::ImageDao> imageDao,
                            std::shared_ptr<dao::TaskDao> taskDao,
                            std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal,
                            std::shared_ptr<infrastructure::gdal::RasterProcessor> rasterProcessor);

        ~FvcService() = default;

        /**
         * @brief 异步提交 FVC 反演任务
         * @param params 反演入参
         * @return 结果 future (建任务记录后后台执行)
         */
        std::future<FvcResult> submitFvcTask(const FvcParams& params);

        /**
         * @brief FVC 反演主流程
         *
         * NDVI -> FVC -> 统计 -> 入库, 回写任务进度 (取 image_ids 首幅影像);
         * 影像缺失或计算失败以失败结果返回。
         * @param params 反演入参
         * @param taskId 关联任务 id
         * @return 反演结果
         */
        FvcResult executeFvc(const FvcParams& params, int taskId);

        /**
         * @brief NDVI 计算
         *
         * NDVI = (NIR-Red)/(NIR+Red), 逐像元计算并截断到 [-1,1]。
         * @param redBandPath 红波段路径
         * @param nirBandPath 近红外波段路径
         * @param outputPath 输出路径
         * @return 计算结果
         */
        NdviResult calculateNdvi(const std::string& redBandPath, const std::string& nirBandPath,
                                 const std::string& outputPath);

        /**
         * @brief 像元二分模型换算 FVC
         *
         * FVC=(NDVI-ndviSoil)/(ndviVeg-ndviSoil), 截断到 [0,1]。
         * @param ndviPath NDVI 栅格路径
         * @param outputPath 输出路径
         * @param ndviSoil 全裸土 NDVI
         * @param ndviVeg 全植被 NDVI
         * @return 计算结果
         */
        FvcResult calculateFvc(const std::string& ndviPath, const std::string& outputPath, double ndviSoil = 0.2,
                               double ndviVeg = 0.8);

        /**
         * @brief 统计均值、像元总数与各 FVC 等级像元数
         * @param fvcPath FVC 栅格路径
         * @return 统计项
         */
        std::map<std::string, double> calculateStatistics(const std::string& fvcPath);

        /**
         * @brief FVC 值分级
         * @param fvc 覆盖度值 [0,1]
         * @return 等级 1-5 (按 0.2/0.4/0.6/0.8 分档)
         */
        static int classifyFvcValue(double fvc);

        /**
         * @brief 按期次查 FVC 成果
         * @param periodId 期次 id
         * @return 成果; 不存在返回 nullopt
         */
        std::optional<models::VegetationCoverage> getFvcByPeriod(int periodId);

        /**
         * @brief 分页列出 FVC 成果
         * @param page 页码, 从 1 开始
         * @param pageSize 每页条数
         * @return 成果列表
         */
        std::vector<models::VegetationCoverage> listFvc(int page = 1, int pageSize = 20);

    private:
        std::shared_ptr<dao::VegetationCoverageDao> fvcDao_;
        std::shared_ptr<dao::ImageDao> imageDao_;
        std::shared_ptr<dao::TaskDao> taskDao_;
        std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal_;
        std::shared_ptr<infrastructure::gdal::RasterProcessor> rasterProcessor_;

        void updateTaskProgress(int taskId, int progress, const std::string& message);
        std::string generateOutputPath(const std::string& baseDir, const std::string& suffix);
    };

}  // namespace soilwatercm::services
