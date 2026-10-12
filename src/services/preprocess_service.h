#pragma once

#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../dao/image_dao.h"
#include "../dao/task_dao.h"
#include "../infrastructure/external_tools/otb_interface.h"
#include "../infrastructure/gdal/gdal_wrapper.h"
#include "../infrastructure/gdal/raster_processor.h"
#include "../models/image.h"
#include "../models/task.h"

namespace soilwatercm::services
{

    /** @brief 预处理入参 */
    struct PreprocessParams
    {
        std::vector<int> image_ids;                  ///< 待处理影像 id 列表
        std::string output_dir;                      ///< 输出目录
        bool radiometric_correction = false;         ///< 是否做辐射校正
        bool geometric_correction = false;           ///< 是否做几何校正
        bool orthorectification = false;             ///< 是否做正射纠正
        std::optional<int> target_epsg;              ///< 目标 CRS EPSG
        std::optional<double> target_resolution;     ///< 目标分辨率
    };

    /** @brief 影像镶嵌入参 */
    struct MosaicParams
    {
        std::vector<int> image_ids;   ///< 参与镶嵌的影像 id 列表
        std::string output_path;      ///< 输出栅格路径
        std::string method = "mean";  ///< 重叠区融合方法
        double nodata_value = 0.0;    ///< nodata 值
    };

    /** @brief 预处理/镶嵌结果 */
    struct PreprocessResult
    {
        bool success = false;                    ///< 是否成功
        std::string message;                     ///< 结果说明/失败原因
        std::vector<std::string> output_files;   ///< 输出文件列表
        std::optional<int> task_id;              ///< 关联任务 id
    };

    /**
     * @brief 影像预处理服务
     *
     * 按开关串联辐射校正 -> 几何校正 -> 正射纠正 -> 重采样, 另支持多图镶嵌;
     * 每步产物在原文件名加后缀。OTB 不可用时辐射校正跳过、
     * 正射纠正回退为 GDAL 重投影。
     */
    class PreprocessService
    {
    public:
        /**
         * @brief 构造, 注入影像/任务 DAO、GDAL、栅格处理器与 OTB 接口
         * @param imageDao 影像 DAO
         * @param taskDao 任务 DAO (进度回写)
         * @param gdal GDAL 封装
         * @param rasterProcessor 栅格处理器 (镶嵌)
         * @param otb OTB 接口 (辐射校正/正射纠正)
         */
        explicit PreprocessService(std::shared_ptr<dao::ImageDao> imageDao, std::shared_ptr<dao::TaskDao> taskDao,
                                   std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal,
                                   std::shared_ptr<infrastructure::gdal::RasterProcessor> rasterProcessor,
                                   std::shared_ptr<infrastructure::OtbInterface> otb);

        ~PreprocessService() = default;

        /**
         * @brief 异步提交预处理任务
         * @param params 预处理入参
         * @return 结果 future (建任务记录后后台执行)
         */
        std::future<PreprocessResult> submitPreprocessTask(const PreprocessParams& params);

        /**
         * @brief 预处理主流程
         *
         * 逐影像按开关串联各校正步骤, 按完成张数回写进度;
         * 单步失败即中止, 结果标记失败并记录原因。
         * @param params 预处理入参
         * @param taskId 关联任务 id
         * @return 处理结果
         */
        PreprocessResult executePreprocess(const PreprocessParams& params, int taskId);

        /**
         * @brief 异步提交镶嵌任务
         * @param params 镶嵌入参
         * @return 结果 future
         */
        std::future<PreprocessResult> submitMosaicTask(const MosaicParams& params);

        /**
         * @brief 多图镶嵌为一张栅格 (RasterProcessor::mosaic)
         * @param params 镶嵌入参
         * @param taskId 关联任务 id
         * @return 镶嵌结果 (影像不存在等异常在内部捕获, 以失败结果返回)
         */
        PreprocessResult executeMosaic(const MosaicParams& params, int taskId);

        /**
         * @brief 辐射定标
         * @param inputPath 输入栅格路径
         * @param outputPath 输出路径
         * @return 处理结果; OTB 可用则调 OTB, 否则原样拷贝输入文件
         */
        PreprocessResult radiometricCorrection(const std::string& inputPath, const std::string& outputPath);

        /**
         * @brief 几何校正 (GDAL 重投影)
         * @param inputPath 输入栅格路径
         * @param outputPath 输出路径
         * @param targetEpsg 目标 CRS EPSG
         * @return 处理结果
         */
        PreprocessResult geometricCorrection(const std::string& inputPath, const std::string& outputPath,
                                             int targetEpsg);

        /**
         * @brief 正射纠正
         * @param inputPath 输入栅格路径
         * @param outputPath 输出路径
         * @param demPath DEM 路径; 可空
         * @return 处理结果; OTB 可用则调 OTB, 否则回退 GDAL 重投影到 4326
         */
        PreprocessResult orthorectification(const std::string& inputPath, const std::string& outputPath,
                                            const std::optional<std::string>& demPath = std::nullopt);

        /**
         * @brief 重采样到目标分辨率 (双线性)
         * @param inputPath 输入栅格路径
         * @param outputPath 输出路径
         * @param targetResolution 目标分辨率
         * @return 处理结果
         */
        PreprocessResult resample(const std::string& inputPath, const std::string& outputPath, double targetResolution);

        /// @brief OTB 可用性检查
        bool checkOtbAvailable();

        /// @brief GDAL 可用性检查
        bool checkGdalAvailable();

    private:
        std::shared_ptr<dao::ImageDao> imageDao_;
        std::shared_ptr<dao::TaskDao> taskDao_;
        std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal_;
        std::shared_ptr<infrastructure::gdal::RasterProcessor> rasterProcessor_;
        std::shared_ptr<infrastructure::OtbInterface> otb_;

        std::string generateOutputPath(const std::string& inputPath, const std::string& suffix);
        void updateTaskProgress(int taskId, int progress, const std::string& message);
    };

}  // namespace soilwatercm::services
