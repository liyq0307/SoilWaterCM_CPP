#pragma once
#define _USE_MATH_DEFINES
#include <cmath>
#include <future>
#include <numbers>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "../gdal/gdal_wrapper.h"

namespace soilwatercm::infrastructure
{

    using soilwatercm::infrastructure::gdal::RasterData;

    /// 形变计算参数
    struct DeformationParams
    {
        double wavelength = 0.05546576;
        double incidenceAngle = 39.0;
        double headingAngle = 0.0;
        std::string crs = "EPSG:4326";
    };

    /// 形变计算结果 (LOS 向/垂直向/速度/误差)
    struct DeformationResult
    {
        bool success;
        std::string errorMessage;
        RasterData losDeformation;
        RasterData verticalDeformation;
        RasterData velocity;
        RasterData error;
    };

    /**
     * @brief 形变计算
     *
     * 解缠相位 -> LOS 向形变 -> 垂直向形变, 支持时序速度估计和地理编码。
     */
    class DeformationCalculator
    {
    public:
        DeformationCalculator();

        ~DeformationCalculator() = default;

        /**
         * @brief 完整流程: 相位转形变、转垂直向、估算误差
         * @param unwrappedPhase 解缠相位
         * @param params 雷达与几何参数
         * @return 形变结果
         */
        DeformationResult calculate(const RasterData& unwrappedPhase,
                                    const DeformationParams& params = DeformationParams{});

        /// @brief 相位转距离: d = -λ·φ / 4π
        RasterData phaseToDeformation(const RasterData& phase, double wavelength);

        /// @brief LOS 形变除以入射角余弦得垂直分量
        RasterData losToVertical(const RasterData& losDeformation, double incidenceAngle);

        /// @brief 时序形变逐像素线性回归, 斜率即形变速度
        RasterData calculateVelocity(const std::vector<RasterData>& timeSeriesDeformations,
                                     const std::vector<double>& times);

        /// @brief Range-Doppler 模型地理编码到 DEM 网格
        RasterData geocode(const RasterData& rangeDopplerData, const RasterData& dem, const DeformationParams& params);

        /// @brief 异步版 calculate
        std::future<DeformationResult> calculateAsync(const RasterData& unwrappedPhase,
                                                      const DeformationParams& params);

        /// @brief 设置默认雷达波长
        void setWavelength(double wavelength) { default_params_.wavelength = wavelength; }

        /// @brief 设置默认入射角 (度)
        void setIncidenceAngle(double angle) { default_params_.incidenceAngle = angle; }

    private:
        std::shared_ptr<spdlog::logger> logger_;
        DeformationParams default_params_;

        double phaseToDistance(double phase, double wavelength)
        {
            return -wavelength * phase / (4.0 * std::numbers::pi);
        }

        double losToVerticalDistance(double los, double incidenceAngle)
        {
            double incRad = incidenceAngle * std::numbers::pi / 180.0;
            return los / std::cos(incRad);
        }

        RasterData estimateError(const RasterData& deformation, const RasterData& coherence);
    };

}  // namespace soilwatercm::infrastructure
