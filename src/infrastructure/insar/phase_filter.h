#pragma once

#include <future>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "../gdal/gdal_wrapper.h"

namespace soilwatercm::infrastructure
{

    using soilwatercm::infrastructure::gdal::RasterData;

    /// 相位滤波方法
    enum class FilterMethod
    {
        Goldstein,
        Mean,
        Median,
        Gaussian
    };

    /// 滤波参数
    struct FilterParams
    {
        FilterMethod method = FilterMethod::Goldstein;
        double alpha = 0.5;
        int windowSize = 32;
        int overlap = 8;
        double sigma = 1.0;
    };

    /**
     * @brief 干涉图相位滤波
     *
     * Goldstein 频域滤波为主, 另提供均值/中值/高斯空间滤波。
     */
    class PhaseFilter
    {
    public:
        PhaseFilter();

        ~PhaseFilter() = default;

        /// @brief 按 params.method 分派到对应滤波
        RasterData filter(const RasterData& phase, const RasterData& coherence,
                          const FilterParams& params = FilterParams{});

        /**
         * @brief Goldstein 滤波
         *
         * 分块 FFT, 按功率谱加权后再逆变换, alpha 控制滤波强度。
         * @param phase 干涉相位
         * @param coherence 相干性图 (用作权重)
         * @param alpha 滤波强度
         * @param windowSize FFT 分块大小
         * @param overlap 分块重叠像素
         * @return 滤波后相位
         */
        RasterData goldsteinFilter(const RasterData& phase, const RasterData& coherence, double alpha = 0.5,
                                   int windowSize = 32, int overlap = 8);

        /// @brief 均值滤波
        RasterData meanFilter(const RasterData& phase, int windowSize = 5);

        /// @brief 中值滤波
        RasterData medianFilter(const RasterData& phase, int windowSize = 5);

        /// @brief 高斯滤波
        RasterData gaussianFilter(const RasterData& phase, double sigma = 1.0);

        /// @brief 异步版 filter
        std::future<RasterData> filterAsync(const RasterData& phase, const RasterData& coherence,
                                            const FilterParams& params);

        /// @brief 设置并行线程数
        void setNumThreads(int numThreads) { num_threads_ = numThreads; }

    private:
        std::shared_ptr<spdlog::logger> logger_;
        int num_threads_;

        std::vector<float> fft1D(const std::vector<float>& input);
        std::vector<float> ifft1D(const std::vector<float>& input);

        std::vector<std::vector<float>> fft2D(const std::vector<std::vector<float>>& input);
        std::vector<std::vector<float>> ifft2D(const std::vector<std::vector<float>>& input);

        std::vector<float> applyFrequencyWeight(const std::vector<float>& spectrum, double alpha);

        double wrapPhase(double phase);
    };

}  // namespace soilwatercm::infrastructure
