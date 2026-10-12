#define _USE_MATH_DEFINES
#include "complex_image_processor.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "../../utils/logger.h"

namespace soilwatercm::infrastructure
{

    ComplexImageProcessor::ComplexImageProcessor() : logger_(utils::Logger::getLogger("complex_image_processor"))
    {
        logger_->info("Complex image processor initialized");
    }

    std::vector<std::complex<float>> ComplexImageProcessor::readComplexRaster(const std::string& filename, int bandReal,
                                                                              int bandImag)
    {
        std::vector<std::complex<float>> data;

        GdalWrapper gdal;
        RasterData realPart = gdal.readRaster(filename, bandReal);

        if (realPart.data.empty())
        {
            logger_->error("Failed to read complex raster: {}", filename);
            return data;
        }

        size_t totalPixels = realPart.width * realPart.height;
        data.resize(totalPixels);

        RasterData imagPart = gdal.readRaster(filename, bandImag);

        if (imagPart.data.empty())
        {
            logger_->info("No imaginary part found, treating as phase data");
            for (size_t i = 0; i < totalPixels; ++i)
            {
                float phaseVal = static_cast<float>(realPart.data[i]);
                data[i] = std::complex<float>(std::cos(phaseVal), std::sin(phaseVal));
            }
        }
        else
        {
            for (size_t i = 0; i < totalPixels; ++i)
            {
                data[i] =
                    std::complex<float>(static_cast<float>(realPart.data[i]), static_cast<float>(imagPart.data[i]));
            }
        }

        logger_->info("Read complex raster: {} pixels", totalPixels);
        return data;
    }

    bool ComplexImageProcessor::writeComplexRaster(const std::string& filename,
                                                   const std::vector<std::complex<float>>& data, int width, int height)
    {
        std::vector<double> realPart(data.size());
        std::vector<double> imagPart(data.size());

        for (size_t i = 0; i < data.size(); ++i)
        {
            realPart[i] = data[i].real();
            imagPart[i] = data[i].imag();
        }

        GdalWrapper gdal;
        RasterData realData;
        realData.width = width;
        realData.height = height;
        realData.data = realPart;

        if (!gdal.writeRaster(filename, realData))
        {
            logger_->error("Failed to write complex raster: {}", filename);
            return false;
        }

        logger_->info("Wrote complex raster: {}", filename);
        return true;
    }

    std::vector<std::complex<float>> ComplexImageProcessor::add(const std::vector<std::complex<float>>& a,
                                                                const std::vector<std::complex<float>>& b)
    {
        if (a.size() != b.size())
        {
            logger_->error("Size mismatch in complex addition");
            return {};
        }

        std::vector<std::complex<float>> result(a.size());
        for (size_t i = 0; i < a.size(); ++i)
        {
            result[i] = a[i] + b[i];
        }
        return result;
    }

    std::vector<std::complex<float>> ComplexImageProcessor::subtract(const std::vector<std::complex<float>>& a,
                                                                     const std::vector<std::complex<float>>& b)
    {
        if (a.size() != b.size())
        {
            logger_->error("Size mismatch in complex subtraction");
            return {};
        }

        std::vector<std::complex<float>> result(a.size());
        for (size_t i = 0; i < a.size(); ++i)
        {
            result[i] = a[i] - b[i];
        }
        return result;
    }

    std::vector<std::complex<float>> ComplexImageProcessor::multiply(const std::vector<std::complex<float>>& a,
                                                                     const std::vector<std::complex<float>>& b)
    {
        if (a.size() != b.size())
        {
            logger_->error("Size mismatch in complex multiplication");
            return {};
        }

        std::vector<std::complex<float>> result(a.size());
        for (size_t i = 0; i < a.size(); ++i)
        {
            result[i] = a[i] * b[i];
        }
        return result;
    }

    std::vector<std::complex<float>> ComplexImageProcessor::divide(const std::vector<std::complex<float>>& a,
                                                                   const std::vector<std::complex<float>>& b)
    {
        if (a.size() != b.size())
        {
            logger_->error("Size mismatch in complex division");
            return {};
        }

        std::vector<std::complex<float>> result(a.size());
        for (size_t i = 0; i < a.size(); ++i)
        {
            if (std::abs(b[i]) < 1e-10)
            {
                result[i] = std::complex<float>(0.0f, 0.0f);
            }
            else
            {
                result[i] = a[i] / b[i];
            }
        }
        return result;
    }

    std::vector<std::complex<float>> ComplexImageProcessor::conjugate(const std::vector<std::complex<float>>& input)
    {
        std::vector<std::complex<float>> result(input.size());
        for (size_t i = 0; i < input.size(); ++i)
        {
            result[i] = std::conj(input[i]);
        }
        return result;
    }

    std::vector<std::complex<float>> ComplexImageProcessor::interferogram(
        const std::vector<std::complex<float>>& master, const std::vector<std::complex<float>>& slave)
    {
        return multiply(master, conjugate(slave));
    }

    std::vector<float> ComplexImageProcessor::amplitude(const std::vector<std::complex<float>>& input)
    {
        std::vector<float> result(input.size());
        for (size_t i = 0; i < input.size(); ++i)
        {
            result[i] = std::abs(input[i]);
        }
        return result;
    }

    std::vector<float> ComplexImageProcessor::phase(const std::vector<std::complex<float>>& input)
    {
        std::vector<float> result(input.size());
        for (size_t i = 0; i < input.size(); ++i)
        {
            result[i] = std::atan2(input[i].imag(), input[i].real());
        }
        return result;
    }

    std::vector<float> ComplexImageProcessor::intensity(const std::vector<std::complex<float>>& input)
    {
        std::vector<float> result(input.size());
        for (size_t i = 0; i < input.size(); ++i)
        {
            result[i] = std::norm(input[i]);
        }
        return result;
    }

    ComplexStats ComplexImageProcessor::statistics(const std::vector<std::complex<float>>& input)
    {
        ComplexStats stats;

        if (input.empty())
        {
            return stats;
        }

        double sumReal = 0.0;
        double sumImag = 0.0;
        double sumAmp = 0.0;
        double sumPhase = 0.0;

        for (const auto& z : input)
        {
            sumReal += z.real();
            sumImag += z.imag();
            sumAmp += std::abs(z);
            sumPhase += std::atan2(z.imag(), z.real());
        }

        stats.meanReal = sumReal / input.size();
        stats.meanImag = sumImag / input.size();
        stats.meanAmplitude = sumAmp / input.size();
        stats.meanPhase = sumPhase / input.size();

        double sumDiff2 = 0.0;
        std::complex<double> mean(stats.meanReal, stats.meanImag);

        for (const auto& z : input)
        {
            std::complex<double> diff = std::complex<double>(z.real(), z.imag()) - mean;
            sumDiff2 += std::norm(diff);
        }

        stats.stdDev = std::sqrt(sumDiff2 / input.size());

        return stats;
    }

    double ComplexImageProcessor::coherence(const std::vector<std::complex<float>>& master,
                                            const std::vector<std::complex<float>>& slave, int windowSize)
    {
        if (master.size() != slave.size() || master.empty())
        {
            return 0.0;
        }

        std::complex<double> sumInterferogram(0.0, 0.0);
        double sumMasterAmp2 = 0.0;
        double sumSlaveAmp2 = 0.0;

        for (size_t i = 0; i < master.size(); ++i)
        {
            std::complex<double> m(master[i].real(), master[i].imag());
            std::complex<double> s(slave[i].real(), slave[i].imag());

            sumInterferogram += m * std::conj(s);
            sumMasterAmp2 += std::norm(m);
            sumSlaveAmp2 += std::norm(s);
        }

        if (sumMasterAmp2 > 0 && sumSlaveAmp2 > 0)
        {
            return std::abs(sumInterferogram) / std::sqrt(sumMasterAmp2 * sumSlaveAmp2);
        }

        return 0.0;
    }

    std::vector<std::complex<float>> ComplexImageProcessor::crop(const std::vector<std::complex<float>>& input,
                                                                 int width, int height, int x0, int y0, int cropWidth,
                                                                 int cropHeight)
    {
        std::vector<std::complex<float>> result(cropWidth * cropHeight);

        for (int y = 0; y < cropHeight; ++y)
        {
            for (int x = 0; x < cropWidth; ++x)
            {
                int srcX = x0 + x;
                int srcY = y0 + y;

                if (srcX >= 0 && srcX < width && srcY >= 0 && srcY < height)
                {
                    result[y * cropWidth + x] = input[srcY * width + srcX];
                }
                else
                {
                    result[y * cropWidth + x] = std::complex<float>(0.0f, 0.0f);
                }
            }
        }

        return result;
    }

    std::vector<std::complex<float>> ComplexImageProcessor::resample(const std::vector<std::complex<float>>& input,
                                                                     int srcWidth, int srcHeight, int dstWidth,
                                                                     int dstHeight)
    {
        std::vector<std::complex<float>> result(dstWidth * dstHeight);

        double scaleX = static_cast<double>(srcWidth) / dstWidth;
        double scaleY = static_cast<double>(srcHeight) / dstHeight;

        for (int y = 0; y < dstHeight; ++y)
        {
            for (int x = 0; x < dstWidth; ++x)
            {
                int srcX = static_cast<int>(x * scaleX);
                int srcY = static_cast<int>(y * scaleY);

                srcX = std::min(srcX, srcWidth - 1);
                srcY = std::min(srcY, srcHeight - 1);

                result[y * dstWidth + x] = input[srcY * srcWidth + srcX];
            }
        }

        return result;
    }

    double ComplexImageProcessor::phaseDerivativeVariance(const std::vector<float>& phase, int width, int height)
    {
        double sumDiff2 = 0.0;
        int count = 0;

        for (int y = 0; y < height - 1; ++y)
        {
            for (int x = 0; x < width - 1; ++x)
            {
                int idx = y * width + x;

                double dx = wrapPhase(phase[idx + 1] - phase[idx]);
                double dy = wrapPhase(phase[idx + width] - phase[idx]);

                sumDiff2 += dx * dx + dy * dy;
                count += 2;
            }
        }

        return sumDiff2 / count;
    }

    int ComplexImageProcessor::countResidues(const std::vector<float>& phase, int width, int height)
    {
        int residues = 0;

        for (int y = 0; y < height - 1; ++y)
        {
            for (int x = 0; x < width - 1; ++x)
            {
                int idx = y * width + x;

                double d1 = wrapPhase(phase[idx + 1] - phase[idx]);
                double d2 = wrapPhase(phase[idx + width + 1] - phase[idx + 1]);
                double d3 = wrapPhase(phase[idx + width] - phase[idx + width + 1]);
                double d4 = wrapPhase(phase[idx] - phase[idx + width]);

                double sum = d1 + d2 + d3 + d4;

                if (sum > M_PI)
                {
                    residues++;
                }
                else if (sum < -M_PI)
                {
                    residues--;
                }
            }
        }

        return residues;
    }

    double ComplexImageProcessor::wrapPhase(double phase)
    {
        while (phase > M_PI)
            phase -= 2 * M_PI;
        while (phase < -M_PI)
            phase += 2 * M_PI;
        return phase;
    }

}  // namespace soilwatercm::infrastructure
