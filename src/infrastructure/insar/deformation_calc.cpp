#include "deformation_calc.h"

#include <cmath>
#include <future>
#include <numeric>

#include "utils/logger.h"

namespace soilwatercm::infrastructure
{

    DeformationCalculator::DeformationCalculator() : logger_(utils::Logger::getLogger("deformation_calc"))
    {
        logger_->info("Deformation calculator initialized");
    }

    DeformationResult DeformationCalculator::calculate(const RasterData& unwrappedPhase,
                                                       const DeformationParams& params)
    {
        DeformationResult result;

        logger_->info("Calculating deformation from unwrapped phase");
        logger_->info("  Wavelength: {} m", params.wavelength);
        logger_->info("  Incidence angle: {} deg", params.incidenceAngle);

        result.losDeformation = phaseToDeformation(unwrappedPhase, params.wavelength);

        result.verticalDeformation = losToVertical(result.losDeformation, params.incidenceAngle);

        result.verticalDeformation.width = unwrappedPhase.width;
        result.verticalDeformation.height = unwrappedPhase.height;

        result.error = estimateError(result.verticalDeformation, unwrappedPhase);

        result.success = true;
        logger_->info("Deformation calculation completed");

        return result;
    }

    RasterData DeformationCalculator::phaseToDeformation(const RasterData& phase, double wavelength)
    {
        RasterData deformation;
        deformation.width = phase.width;
        deformation.height = phase.height;
        deformation.data.resize(phase.data.size());

        double scaleFactor = -wavelength / (4.0 * std::numbers::pi);

        for (size_t i = 0; i < phase.data.size(); ++i)
        {
            deformation.data[i] = static_cast<float>(phase.data[i] * scaleFactor);
        }

        logger_->info("Phase to deformation conversion completed, scale factor: {}", scaleFactor);
        return deformation;
    }

    RasterData DeformationCalculator::losToVertical(const RasterData& losDeformation, double incidenceAngle)
    {
        RasterData vertical;
        vertical.width = losDeformation.width;
        vertical.height = losDeformation.height;
        vertical.data.resize(losDeformation.data.size());

        double incRad = incidenceAngle * std::numbers::pi / 180.0;
        double scaleFactor = 1.0 / std::cos(incRad);

        for (size_t i = 0; i < losDeformation.data.size(); ++i)
        {
            vertical.data[i] = static_cast<float>(losDeformation.data[i] * scaleFactor);
        }

        logger_->info("LOS to vertical conversion completed, scale factor: {}", scaleFactor);
        return vertical;
    }

    RasterData DeformationCalculator::calculateVelocity(const std::vector<RasterData>& timeSeriesDeformations,
                                                        const std::vector<double>& times)
    {
        if (timeSeriesDeformations.empty() || times.empty())
        {
            logger_->error("Empty time series data");
            return RasterData();
        }

        if (timeSeriesDeformations.size() != times.size())
        {
            logger_->error("Time series size mismatch");
            return RasterData();
        }

        RasterData velocity;
        velocity.width = timeSeriesDeformations[0].width;
        velocity.height = timeSeriesDeformations[0].height;
        velocity.data.resize(velocity.width * velocity.height);

        size_t n = times.size();
        double sumT = std::accumulate(times.begin(), times.end(), 0.0);
        double sumT2 = 0.0;
        for (double t : times)
        {
            sumT2 += t * t;
        }

        double denom = n * sumT2 - sumT * sumT;

        for (size_t i = 0; i < velocity.data.size(); ++i)
        {
            double sumD = 0.0;
            double sumTD = 0.0;

            for (size_t j = 0; j < n; ++j)
            {
                double d = timeSeriesDeformations[j].data[i];
                sumD += d;
                sumTD += times[j] * d;
            }

            double slope = (n * sumTD - sumT * sumD) / denom;

            velocity.data[i] = static_cast<float>(slope);
        }

        logger_->info("Velocity calculation completed from {} time points", n);
        return velocity;
    }

    RasterData DeformationCalculator::geocode(const RasterData& rangeDopplerData, const RasterData& dem,
                                              const DeformationParams& params)
    {
        logger_->info("Performing Range-Doppler geocoding");

        RasterData geocoded;
        geocoded.width = dem.width;
        geocoded.height = dem.height;
        geocoded.data.resize(dem.width * dem.height, 0.0f);
        geocoded.geotransform[0] = dem.geotransform[0];
        geocoded.geotransform[1] = dem.geotransform[1];
        geocoded.geotransform[2] = dem.geotransform[2];
        geocoded.geotransform[3] = dem.geotransform[3];
        geocoded.geotransform[4] = dem.geotransform[4];
        geocoded.geotransform[5] = dem.geotransform[5];
        geocoded.projection = dem.projection;

        if (rangeDopplerData.width == 0 || rangeDopplerData.height == 0)
        {
            logger_->warn("Range-Doppler data is empty, returning zero-filled raster");
            return geocoded;
        }

        double rdCellX = rangeDopplerData.width > 1 ? 1.0 : 1.0;
        double rdCellY = rangeDopplerData.height > 1 ? 1.0 : 1.0;
        (void)rdCellX;
        (void)rdCellY;

        double scaleX = static_cast<double>(rangeDopplerData.width) / dem.width;
        double scaleY = static_cast<double>(rangeDopplerData.height) / dem.height;

        for (int demRow = 0; demRow < dem.height; ++demRow)
        {
            for (int demCol = 0; demCol < dem.width; ++demCol)
            {
                int rdCol = static_cast<int>(demCol * scaleX);
                int rdRow = static_cast<int>(demRow * scaleY);

                rdCol = std::max(0, std::min(rdCol, rangeDopplerData.width - 1));
                rdRow = std::max(0, std::min(rdRow, rangeDopplerData.height - 1));

                int rdIdx = rdRow * rangeDopplerData.width + rdCol;
                int demIdx = demRow * dem.width + demCol;

                if (rdIdx >= 0 && rdIdx < static_cast<int>(rangeDopplerData.data.size()))
                {
                    geocoded.data[demIdx] = rangeDopplerData.data[rdIdx];
                }
            }
        }

        logger_->info("Geocoding completed: {}x{} -> {}x{}", rangeDopplerData.width, rangeDopplerData.height,
                      geocoded.width, geocoded.height);
        return geocoded;
    }

    std::future<DeformationResult> DeformationCalculator::calculateAsync(const RasterData& unwrappedPhase,
                                                                         const DeformationParams& params)
    {
        return std::async(std::launch::async,
                          [this, unwrappedPhase, params]() { return calculate(unwrappedPhase, params); });
    }

    RasterData DeformationCalculator::estimateError(const RasterData& deformation, const RasterData& coherence)
    {
        RasterData error;
        error.width = deformation.width;
        error.height = deformation.height;
        error.data.resize(deformation.data.size());

        for (size_t i = 0; i < deformation.data.size(); ++i)
        {
            double coh = std::abs(coherence.data[i]);

            if (coh < 0.1)
            {
                error.data[i] = std::abs(deformation.data[i]) * 0.5f;
            }
            else if (coh < 0.3)
            {
                error.data[i] = std::abs(deformation.data[i]) * 0.2f;
            }
            else if (coh < 0.5)
            {
                error.data[i] = std::abs(deformation.data[i]) * 0.1f;
            }
            else
            {
                error.data[i] = std::abs(deformation.data[i]) * 0.05f;
            }
        }

        return error;
    }

}  // namespace soilwatercm::infrastructure
