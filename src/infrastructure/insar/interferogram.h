#pragma once

#include <complex>
#include <future>
#include <memory>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "../gdal/gdal_wrapper.h"

namespace soilwatercm::infrastructure
{

    using soilwatercm::infrastructure::gdal::GdalWrapper;
    using soilwatercm::infrastructure::gdal::RasterData;

    /// 干涉图生成结果 (相位/幅度/相干性三张图)
    struct InterferogramResult
    {
        bool success;
        std::string errorMessage;
        RasterData phase;
        RasterData amplitude;
        RasterData coherence;
    };

    /**
     * @brief 干涉图生成
     *
     * 主影像与共轭辅影像逐像素相乘得到干涉相位, 幅度取主影像,
     * 相干性由窗口内互相关估计。
     */
    class InterferogramGenerator
    {
    public:
        InterferogramGenerator();

        ~InterferogramGenerator() = default;

        /**
         * @brief 从两组复数影像计算干涉图
         * @param masterImage 主影像复数数据
         * @param slaveImage 辅影像复数数据 (尺寸须一致)
         * @param width 影像宽
         * @param height 影像高
         * @return 干涉图结果
         */
        InterferogramResult generate(const std::vector<std::complex<float>>& masterImage,
                                     const std::vector<std::complex<float>>& slaveImage, int width, int height);

        /**
         * @brief 从复数影像文件生成, 相位/幅度/相干性写到 outputDir
         * @param masterFile 主影像文件
         * @param slaveFile 辅影像文件
         * @param outputDir 输出目录
         * @return 干涉图结果
         */
        InterferogramResult generateFromFiles(const std::string& masterFile, const std::string& slaveFile,
                                              const std::string& outputDir);

        /// @brief 异步版 generate
        std::future<InterferogramResult> generateAsync(const std::vector<std::complex<float>>& masterImage,
                                                       const std::vector<std::complex<float>>& slaveImage, int width,
                                                       int height);

        /**
         * @brief 窗口内互相关估计相干性
         * @param masterImage 主影像复数数据
         * @param slaveImage 辅影像复数数据
         * @param width 影像宽
         * @param height 影像高
         * @param windowSize 滤波窗大小
         * @return 相干性栅格 (0~1)
         */
        RasterData calculateCoherence(const std::vector<std::complex<float>>& masterImage,
                                      const std::vector<std::complex<float>>& slaveImage, int width, int height,
                                      int windowSize = 5);

        /// @brief 设置并行线程数
        void setNumThreads(int numThreads) { num_threads_ = numThreads; }

        /// @brief 当前并行线程数
        int getNumThreads() const { return num_threads_; }

    private:
        std::shared_ptr<spdlog::logger> logger_;
        int num_threads_;

        std::vector<std::complex<float>> readComplexImage(const std::string& filename);

        bool writeComplexImage(const std::string& filename, const std::vector<std::complex<float>>& image, int width,
                               int height);

        std::complex<float> conjugate(const std::complex<float>& z) { return std::conj(z); }

        float phase(const std::complex<float>& z) { return std::atan2(z.imag(), z.real()); }

        float amplitude(const std::complex<float>& z) { return std::abs(z); }
    };

}  // namespace soilwatercm::infrastructure
