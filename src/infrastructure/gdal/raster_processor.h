#pragma once

#include <string>

#include "gdal_wrapper.h"

namespace soilwatercm::infrastructure::gdal
{

    /**
     * @brief 栅格处理
     *
     * 重采样、裁剪、镶嵌、波段运算等常用操作。内存版的 clip/mosaic/fillNodata
     * 当前是桩实现, 直接原样返回; 文件版接口走 GDAL 真实处理。
     */
    class RasterProcessor
    {
    public:
        RasterProcessor();
        ~RasterProcessor() = default;

        /**
         * @brief 重采样到目标尺寸
         *
         * 当前逐像元最近邻取值, method 参数未生效。
         * @param input 输入栅格
         * @param target_width 目标宽
         * @param target_height 目标高
         * @param method 插值方式 (保留参数, 暂不使用)
         * @return 重采样结果
         */
        RasterData resample(const RasterData& input, int target_width, int target_height,
                            const std::string& method = "bilinear");

        /// @brief 按地理范围裁剪 (桩实现, 原样返回)
        RasterData clip(const RasterData& input, double x_min, double y_min, double x_max, double y_max);

        /// @brief 镶嵌 (桩实现, 只返回第一幅)
        RasterData mosaic(const std::vector<RasterData>& inputs);

        /**
         * @brief 逐像元表达式运算
         * @param input 输入栅格
         * @param expression log/exp/sqrt/abs 之一
         * @return 运算结果
         */
        RasterData calculate(const RasterData& input, const std::string& expression);

        /**
         * @brief 两栅格逐像元四则运算
         * @param input1 栅格1
         * @param input2 栅格2 (尺寸须与栅格1一致, 否则抛异常)
         * @param expression + / - * / 组合
         * @return 运算结果
         */
        RasterData calculate(const RasterData& input1, const RasterData& input2, const std::string& expression);

        /// @brief 线性拉伸到 [min_val, max_val]
        RasterData normalize(const RasterData& input, double min_val = 0.0, double max_val = 1.0);

        /// @brief 填充无效值 (桩实现, 原样返回)
        RasterData fillNodata(const RasterData& input, double fill_value);

        /// @brief 返回 min/max/mean/std/sum 五个统计量
        std::vector<double> getStatistics(const RasterData& input);

        /**
         * @brief 邻域滤波 (mean/median/gaussian 卷积核)
         * @param input 输入栅格
         * @param filter_type 滤波类型
         * @param kernel_size 卷积核边长, 默认 3
         * @return 滤波结果; 边缘半核宽度不处理
         */
        RasterData applyFilter(const RasterData& input, const std::string& filter_type, int kernel_size = 3);

        // 文件版接口: 直接读写磁盘栅格

        /**
         * @brief 文件重采样到目标分辨率 (GDALReprojectImage, 双线性)
         * @param inputPath 输入文件
         * @param outputPath 输出文件 (GTiff)
         * @param targetResolution 目标分辨率 (与源仿射同单位)
         * @param method 插值方式 (保留参数, 暂不使用)
         */
        void resample(const std::string& inputPath, const std::string& outputPath, double targetResolution,
                      const std::string& method = "bilinear");

        /**
         * @brief 文件镶嵌, 重叠区按 method (first/last/max/min/mean) 取值
         * @param inputPaths 输入文件列表 (全部打不开则抛异常)
         * @param outputPath 输出文件
         * @param method 重叠区取值方式
         * @param nodataValue 无效值
         */
        void mosaic(const std::vector<std::string>& inputPaths, const std::string& outputPath,
                    const std::string& method = "first", double nodataValue = -9999.0);
    };

}  // namespace soilwatercm::infrastructure::gdal
