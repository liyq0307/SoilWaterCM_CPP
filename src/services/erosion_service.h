#pragma once

#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../dao/erosion_result_dao.h"
#include "../dao/task_dao.h"
#include "../infrastructure/external_tools/grass_interface.h"
#include "../infrastructure/external_tools/saga_interface.h"
#include "../infrastructure/gdal/gdal_wrapper.h"
#include "../infrastructure/gdal/raster_processor.h"
#include "../models/erosion_result.h"

namespace soilwatercm::services
{

    /** @brief USLE 侵蚀计算入参 */
    struct ErosionParams
    {
        int period_id;                                  ///< 期次 id
        std::string dem_path;                           ///< DEM 栅格路径
        std::string rainfall_path;                      ///< 降雨栅格路径
        std::string soil_path;                          ///< 土壤质地栅格路径
        std::string fvc_path;                           ///< 植被覆盖度栅格路径
        std::string output_dir;                         ///< 输出目录
        std::optional<std::string> ls_method = "grass"; ///< LS 算法选择 (grass/saga)
    };

    /** @brief 侵蚀计算结果 */
    struct ErosionResult
    {
        bool success = false;                                  ///< 是否成功
        std::string message;                                   ///< 结果说明/失败原因
        std::optional<std::string> modulus_raster_path;        ///< 侵蚀模数栅格路径
        std::optional<std::string> class_raster_path;          ///< 侵蚀分级栅格路径
        double mean_modulus = 0.0;                             ///< 平均侵蚀模数
        std::map<std::string, double> class_areas;             ///< 各分级面积
        std::optional<int> task_id;                            ///< 关联任务 id
    };

    /**
     * @brief 土壤侵蚀计算服务 (USLE: A = R·K·LS·C·P)
     *
     * 逐因子生成栅格后相乘得侵蚀模数, 再按模数分级出图并统计;
     * 外部工具 (GRASS/SAGA) 不可用时走内置公式回退。
     */
    class ErosionService
    {
    public:
        /**
         * @brief 构造, 注入侵蚀/任务 DAO、GDAL、栅格处理器与 GRASS/SAGA 接口
         * @param erosionDao 侵蚀结果 DAO (入库)
         * @param taskDao 任务 DAO (进度回写)
         * @param gdal GDAL 封装 (逐像元运算)
         * @param rasterProcessor 栅格处理器 (相乘/统计)
         * @param grass GRASS 接口 (LS 因子)
         * @param saga SAGA 接口 (K/LS 因子)
         */
        explicit ErosionService(std::shared_ptr<dao::ErosionResultDao> erosionDao,
                                std::shared_ptr<dao::TaskDao> taskDao,
                                std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal,
                                std::shared_ptr<infrastructure::gdal::RasterProcessor> rasterProcessor,
                                std::shared_ptr<infrastructure::external_tools::GrassInterface> grass,
                                std::shared_ptr<infrastructure::SagaInterface> saga);

        ~ErosionService() = default;

        /**
         * @brief 异步提交侵蚀计算任务
         * @param params 计算入参
         * @return 结果 future (建任务记录后后台执行)
         */
        std::future<ErosionResult> submitErosionTask(const ErosionParams& params);

        /**
         * @brief 侵蚀计算主流程
         *
         * R/K/LS/C/P 五因子 -> USLE 模数 -> 分级 -> 统计 -> 入库, 回写任务进度。
         * @param params 计算入参
         * @param taskId 关联任务 id
         * @return 计算结果 (异常以失败结果返回)
         */
        ErosionResult executeErosion(const ErosionParams& params, int taskId);

        /**
         * @brief R 因子 (降雨侵蚀力)
         *
         * 降雨栅格逐像元套经验公式 R = 0.0483 * P^1.610。
         * @param rainfallPath 降雨栅格路径
         * @param outputPath 输出路径
         * @return 输出路径
         */
        std::string calculateRFactor(const std::string& rainfallPath, const std::string& outputPath);

        /**
         * @brief K 因子 (土壤可蚀性)
         * @param soilPath 土壤质地栅格路径
         * @param outputPath 输出路径
         * @return 输出路径; SAGA 可用则调 SAGA, 否则按质地编码查表 (11 类)
         */
        std::string calculateKFactor(const std::string& soilPath, const std::string& outputPath);

        /**
         * @brief LS 因子 (坡长坡度)
         *
         * 回退算法: 从 DEM 中心差分算坡度, 按坡度分档取 m 指数,
         * L=(坡长/22.13)^m, S 用正弦多项式 65.41sin²θ+4.56sinθ+0.065。
         * @param demPath DEM 路径
         * @param outputPath 输出路径
         * @param method 算法选择 (grass/saga)
         * @return 输出路径; 优先 GRASS/SAGA, 都不可用则走内置回退
         */
        std::string calculateLSFactor(const std::string& demPath, const std::string& outputPath,
                                       const std::string& method = "grass");

        /**
         * @brief C 因子 (植被覆盖)
         *
         * 蔡崇法公式, 由 FVC 栅格逐像元换算。
         * @param fvcPath FVC 栅格路径
         * @param outputPath 输出路径
         * @return 输出路径
         */
        std::string calculateCFactor(const std::string& fvcPath, const std::string& outputPath);

        /**
         * @brief P 因子 (水土保持措施)
         * @param outputPath 输出路径
         * @param pValue P 值, 默认 1.0 (不采取措施)
         * @return 输出路径; 生成 100x100 常量栅格
         */
        std::string calculatePFactor(const std::string& outputPath, double pValue = 1.0);

        /**
         * @brief USLE 模数
         * @param rPath R 因子栅格路径
         * @param kPath K 因子栅格路径
         * @param lsPath LS 因子栅格路径
         * @param cPath C 因子栅格路径
         * @param pPath P 因子栅格路径
         * @param outputPath 输出路径
         * @return 输出路径; 五张栅格逐像元相乘
         */
        std::string calculateUSLE(const std::string& rPath, const std::string& kPath, const std::string& lsPath,
                                  const std::string& cPath, const std::string& pPath, const std::string& outputPath);

        /**
         * @brief 侵蚀分级
         * @param modulusPath 侵蚀模数栅格路径
         * @param outputPath 输出路径
         * @return 输出路径; 按模数 1000/2500/5000/8000/15000 阈值分为 1-6 级
         */
        std::string classifyErosion(const std::string& modulusPath, const std::string& outputPath);

        /**
         * @brief 统计均值与有效像元数
         * @param erosionPath 侵蚀栅格路径
         * @return {mean, valid_pixels} 等统计项
         */
        std::map<std::string, double> calculateStatistics(const std::string& erosionPath);

        /// @brief GRASS 外部工具可用性检查
        bool checkGrassAvailable();

        /// @brief SAGA 外部工具可用性检查
        bool checkSagaAvailable();

    private:
        std::shared_ptr<dao::ErosionResultDao> erosionDao_;
        std::shared_ptr<dao::TaskDao> taskDao_;
        std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal_;
        std::shared_ptr<infrastructure::gdal::RasterProcessor> rasterProcessor_;
        std::shared_ptr<infrastructure::external_tools::GrassInterface> grass_;
        std::shared_ptr<infrastructure::SagaInterface> saga_;

        void updateTaskProgress(int taskId, int progress, const std::string& message);
    };

}  // namespace soilwatercm::services
