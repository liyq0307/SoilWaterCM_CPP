#pragma once

#include <complex>
#include <memory>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "../gdal/gdal_wrapper.h"

namespace soilwatercm::infrastructure
{

    using soilwatercm::infrastructure::gdal::GdalWrapper;
    using soilwatercm::infrastructure::gdal::RasterData;

    /// 复数影像质量指标统计
    struct ComplexStats
    {
        double meanReal = 0.0;
        double meanImag = 0.0;
        double stdDev = 0.0;
        double coherence = 0.0;
        double meanAmplitude = 0.0;
        double meanPhase = 0.0;
    };

    /**
     * @brief SAR 复数影像基础运算
     *
     * 读写复数栅格、四则运算、幅度/相位/强度提取及质量指标统计。
     */
    class ComplexImageProcessor
    {
    public:
        ComplexImageProcessor();

        ~ComplexImageProcessor() = default;

        /**
         * @brief 读复数影像 (按实部/虚部两个波段存取)
         * @param filename 栅格文件
         * @param bandReal 实部波段号
         * @param bandImag 虚部波段号
         * @return 复数数据
         */
        std::vector<std::complex<float>> readComplexRaster(const std::string& filename, int bandReal = 1,
                                                           int bandImag = 2);

        /// @brief 写复数影像 (实部/虚部分两波段)
        bool writeComplexRaster(const std::string& filename, const std::vector<std::complex<float>>& data, int width,
                                int height);

        /// @brief 逐像素复数加法
        std::vector<std::complex<float>> add(const std::vector<std::complex<float>>& a,
                                             const std::vector<std::complex<float>>& b);

        /// @brief 逐像素复数减法
        std::vector<std::complex<float>> subtract(const std::vector<std::complex<float>>& a,
                                                  const std::vector<std::complex<float>>& b);

        /// @brief 逐像素复数乘法
        std::vector<std::complex<float>> multiply(const std::vector<std::complex<float>>& a,
                                                  const std::vector<std::complex<float>>& b);

        /// @brief 逐像素复数除法
        std::vector<std::complex<float>> divide(const std::vector<std::complex<float>>& a,
                                                const std::vector<std::complex<float>>& b);

        /// @brief 逐像素取共轭
        std::vector<std::complex<float>> conjugate(const std::vector<std::complex<float>>& input);

        /// @brief 主影像乘辅影像共轭, 得到干涉图
        std::vector<std::complex<float>> interferogram(const std::vector<std::complex<float>>& master,
                                                       const std::vector<std::complex<float>>& slave);

        /// @brief 提取幅度
        std::vector<float> amplitude(const std::vector<std::complex<float>>& input);

        /// @brief 提取相位 (-π~π)
        std::vector<float> phase(const std::vector<std::complex<float>>& input);

        /// @brief 提取强度 (幅度平方)
        std::vector<float> intensity(const std::vector<std::complex<float>>& input);

        /// @brief 统计全图质量指标
        ComplexStats statistics(const std::vector<std::complex<float>>& input);

        /// @brief 窗口互相关估计两景影像的相干性
        double coherence(const std::vector<std::complex<float>>& master, const std::vector<std::complex<float>>& slave,
                         int windowSize = 5);

        /// @brief 从影像中裁出子块
        std::vector<std::complex<float>> crop(const std::vector<std::complex<float>>& input, int width, int height,
                                              int x0, int y0, int cropWidth, int cropHeight);

        /// @brief 复数影像重采样到目标尺寸
        std::vector<std::complex<float>> resample(const std::vector<std::complex<float>>& input, int srcWidth,
                                                  int srcHeight, int dstWidth, int dstHeight);

        /// @brief 相位相邻差分的方差, 衡量相位噪声水平
        double phaseDerivativeVariance(const std::vector<float>& phase, int width, int height);

        /// @brief 统计 2x2 环路上的残差点数量, 用于解缠质量评估
        int countResidues(const std::vector<float>& phase, int width, int height);

    private:
        std::shared_ptr<spdlog::logger> logger_;

        double wrapPhase(double phase);
    };

}  // namespace soilwatercm::infrastructure
