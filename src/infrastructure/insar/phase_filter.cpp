#include "phase_filter.h"

#include <algorithm>
#include <cmath>
#include <future>
#include <numbers>
#include <numeric>

#include "utils/logger.h"

namespace soilwatercm::infrastructure
{

    PhaseFilter::PhaseFilter()
        : logger_(utils::Logger::getLogger("phase_filter")), num_threads_(std::thread::hardware_concurrency())
    {
        logger_->info("Phase filter initialized, threads: {}", num_threads_);
    }

    RasterData PhaseFilter::filter(const RasterData& phase, const RasterData& coherence, const FilterParams& params)
    {
        logger_->info("Applying phase filter, method: {}", static_cast<int>(params.method));

        switch (params.method)
        {
            case FilterMethod::Goldstein:
                return goldsteinFilter(phase, coherence, params.alpha, params.windowSize, params.overlap);
            case FilterMethod::Mean:
                return meanFilter(phase, params.windowSize);
            case FilterMethod::Median:
                return medianFilter(phase, params.windowSize);
            case FilterMethod::Gaussian:
                return gaussianFilter(phase, params.sigma);
            default:
                logger_->warn("Unknown filter method, using Goldstein");
                return goldsteinFilter(phase, coherence, params.alpha, params.windowSize, params.overlap);
        }
    }

    RasterData PhaseFilter::goldsteinFilter(const RasterData& phase, const RasterData& coherence, double alpha,
                                            int windowSize, int overlap)
    {
        RasterData filtered;
        filtered.width = phase.width;
        filtered.height = phase.height;
        filtered.data.resize(phase.width * phase.height, 0.0f);

        std::vector<float> weight(phase.width * phase.height, 0.0f);

        int step = windowSize - overlap;

        for (int y0 = 0; y0 < phase.height; y0 += step)
        {
            for (int x0 = 0; x0 < phase.width; x0 += step)
            {
                std::vector<std::vector<float>> window(windowSize, std::vector<float>(windowSize, 0.0f));
                std::vector<std::vector<float>> cohWindow(windowSize, std::vector<float>(windowSize, 0.0f));

                for (int dy = 0; dy < windowSize; ++dy)
                {
                    for (int dx = 0; dx < windowSize; ++dx)
                    {
                        int y = y0 + dy;
                        int x = x0 + dx;

                        if (y < phase.height && x < phase.width)
                        {
                            window[dy][dx] = static_cast<float>(phase.data[y * phase.width + x]);
                            cohWindow[dy][dx] = static_cast<float>(coherence.data[y * phase.width + x]);
                        }
                    }
                }

                std::vector<std::vector<float>> spectrum = fft2D(window);

                for (int dy = 0; dy < windowSize; ++dy)
                {
                    for (int dx = 0; dx < windowSize; ++dx)
                    {
                        double mag = std::sqrt(spectrum[dy][dx] * spectrum[dy][dx]);
                        double weightFactor = std::pow(mag + 1e-10, alpha);
                        spectrum[dy][dx] *= static_cast<float>(weightFactor);
                    }
                }

                std::vector<std::vector<float>> filteredWindow = ifft2D(spectrum);

                for (int dy = 0; dy < windowSize; ++dy)
                {
                    for (int dx = 0; dx < windowSize; ++dx)
                    {
                        int y = y0 + dy;
                        int x = x0 + dx;

                        if (y < phase.height && x < phase.width)
                        {
                            double coh = cohWindow[dy][dx];
                            filtered.data[y * phase.width + x] += static_cast<float>(filteredWindow[dy][dx] * coh);
                            weight[y * phase.width + x] += static_cast<float>(coh);
                        }
                    }
                }
            }
        }

        for (size_t i = 0; i < filtered.data.size(); ++i)
        {
            if (weight[i] > 0)
            {
                filtered.data[i] /= weight[i];
            }
            filtered.data[i] = static_cast<float>(wrapPhase(filtered.data[i]));
        }

        logger_->info("Goldstein filter completed");
        return filtered;
    }

    RasterData PhaseFilter::meanFilter(const RasterData& phase, int windowSize)
    {
        RasterData filtered;
        filtered.width = phase.width;
        filtered.height = phase.height;
        filtered.data.resize(phase.width * phase.height);

        int halfWindow = windowSize / 2;

        for (int y = 0; y < phase.height; ++y)
        {
            for (int x = 0; x < phase.width; ++x)
            {
                double sum = 0.0;
                int count = 0;

                for (int dy = -halfWindow; dy <= halfWindow; ++dy)
                {
                    for (int dx = -halfWindow; dx <= halfWindow; ++dx)
                    {
                        int ny = y + dy;
                        int nx = x + dx;

                        if (ny >= 0 && ny < phase.height && nx >= 0 && nx < phase.width)
                        {
                            sum += phase.data[ny * phase.width + nx];
                            count++;
                        }
                    }
                }

                filtered.data[y * phase.width + x] = static_cast<float>(wrapPhase(sum / count));
            }
        }

        return filtered;
    }

    RasterData PhaseFilter::medianFilter(const RasterData& phase, int windowSize)
    {
        RasterData filtered;
        filtered.width = phase.width;
        filtered.height = phase.height;
        filtered.data.resize(phase.width * phase.height);

        int halfWindow = windowSize / 2;

        for (int y = 0; y < phase.height; ++y)
        {
            for (int x = 0; x < phase.width; ++x)
            {
                std::vector<float> values;

                for (int dy = -halfWindow; dy <= halfWindow; ++dy)
                {
                    for (int dx = -halfWindow; dx <= halfWindow; ++dx)
                    {
                        int ny = y + dy;
                        int nx = x + dx;

                        if (ny >= 0 && ny < phase.height && nx >= 0 && nx < phase.width)
                        {
                            values.push_back(static_cast<float>(phase.data[ny * phase.width + nx]));
                        }
                    }
                }

                std::sort(values.begin(), values.end());
                float median = values[values.size() / 2];
                filtered.data[y * phase.width + x] = static_cast<float>(wrapPhase(median));
            }
        }

        return filtered;
    }

    RasterData PhaseFilter::gaussianFilter(const RasterData& phase, double sigma)
    {
        RasterData filtered;
        filtered.width = phase.width;
        filtered.height = phase.height;
        filtered.data.resize(phase.width * phase.height);

        int kernelSize = static_cast<int>(6 * sigma);
        if (kernelSize % 2 == 0)
            kernelSize++;
        int halfKernel = kernelSize / 2;

        std::vector<float> kernel(kernelSize);
        float sum = 0.0f;

        for (int i = 0; i < kernelSize; ++i)
        {
            int x = i - halfKernel;
            kernel[i] = static_cast<float>(std::exp(-x * x / (2 * sigma * sigma)));
            sum += kernel[i];
        }

        for (int i = 0; i < kernelSize; ++i)
        {
            kernel[i] /= sum;
        }

        RasterData temp;
        temp.width = phase.width;
        temp.height = phase.height;
        temp.data.resize(phase.width * phase.height);

        for (int y = 0; y < phase.height; ++y)
        {
            for (int x = 0; x < phase.width; ++x)
            {
                double value = 0.0;
                double weight = 0.0;

                for (int dx = -halfKernel; dx <= halfKernel; ++dx)
                {
                    int nx = x + dx;
                    if (nx >= 0 && nx < phase.width)
                    {
                        value += phase.data[y * phase.width + nx] * kernel[dx + halfKernel];
                        weight += kernel[dx + halfKernel];
                    }
                }

                temp.data[y * phase.width + x] = static_cast<float>(value / weight);
            }
        }

        for (int y = 0; y < phase.height; ++y)
        {
            for (int x = 0; x < phase.width; ++x)
            {
                double value = 0.0;
                double weight = 0.0;

                for (int dy = -halfKernel; dy <= halfKernel; ++dy)
                {
                    int ny = y + dy;
                    if (ny >= 0 && ny < phase.height)
                    {
                        value += temp.data[ny * phase.width + x] * kernel[dy + halfKernel];
                        weight += kernel[dy + halfKernel];
                    }
                }

                filtered.data[y * phase.width + x] = static_cast<float>(wrapPhase(value / weight));
            }
        }

        return filtered;
    }

    std::future<RasterData> PhaseFilter::filterAsync(const RasterData& phase, const RasterData& coherence,
                                                     const FilterParams& params)
    {
        return std::async(std::launch::async,
                          [this, phase, coherence, params]() { return filter(phase, coherence, params); });
    }

    std::vector<float> PhaseFilter::fft1D(const std::vector<float>& input)
    {
        size_t N = input.size();
        std::vector<float> output(N * 2);

        for (size_t k = 0; k < N; ++k)
        {
            double real = 0.0;
            double imag = 0.0;

            for (size_t n = 0; n < N; ++n)
            {
                double angle = -2.0 * std::numbers::pi * k * n / N;
                real += input[n] * std::cos(angle);
                imag += input[n] * std::sin(angle);
            }

            output[2 * k] = static_cast<float>(real);
            output[2 * k + 1] = static_cast<float>(imag);
        }

        return output;
    }

    std::vector<float> PhaseFilter::ifft1D(const std::vector<float>& input)
    {
        size_t N = input.size() / 2;
        std::vector<float> output(N);

        for (size_t n = 0; n < N; ++n)
        {
            double real = 0.0;

            for (size_t k = 0; k < N; ++k)
            {
                double angle = 2.0 * std::numbers::pi * k * n / N;
                real += input[2 * k] * std::cos(angle) - input[2 * k + 1] * std::sin(angle);
            }

            output[n] = static_cast<float>(real / N);
        }

        return output;
    }

    std::vector<std::vector<float>> PhaseFilter::fft2D(const std::vector<std::vector<float>>& input)
    {
        size_t rows = input.size();
        size_t cols = input[0].size();

        std::vector<std::vector<float>> output(rows, std::vector<float>(cols));

        for (size_t y = 0; y < rows; ++y)
        {
            std::vector<float> row = fft1D(input[y]);
            for (size_t x = 0; x < cols; ++x)
            {
                output[y][x] = row[x];
            }
        }

        for (size_t x = 0; x < cols; ++x)
        {
            std::vector<float> col(rows);
            for (size_t y = 0; y < rows; ++y)
            {
                col[y] = output[y][x];
            }

            std::vector<float> colFft = fft1D(col);
            for (size_t y = 0; y < rows; ++y)
            {
                output[y][x] = colFft[y];
            }
        }

        return output;
    }

    std::vector<std::vector<float>> PhaseFilter::ifft2D(const std::vector<std::vector<float>>& input)
    {
        size_t rows = input.size();
        size_t cols = input[0].size();

        std::vector<std::vector<float>> output(rows, std::vector<float>(cols));

        for (size_t x = 0; x < cols; ++x)
        {
            std::vector<float> col(rows);
            for (size_t y = 0; y < rows; ++y)
            {
                col[y] = input[y][x];
            }

            std::vector<float> colIfft = ifft1D(col);
            for (size_t y = 0; y < rows; ++y)
            {
                output[y][x] = colIfft[y];
            }
        }

        for (size_t y = 0; y < rows; ++y)
        {
            std::vector<float> row = ifft1D(output[y]);
            for (size_t x = 0; x < cols; ++x)
            {
                output[y][x] = row[x];
            }
        }

        return output;
    }

    std::vector<float> PhaseFilter::applyFrequencyWeight(const std::vector<float>& spectrum, double alpha)
    {
        std::vector<float> weighted(spectrum.size());

        for (size_t i = 0; i < spectrum.size(); ++i)
        {
            double mag = std::abs(spectrum[i]);
            double weight = std::pow(mag + 1e-10, alpha);
            weighted[i] = static_cast<float>(spectrum[i] * weight);
        }

        return weighted;
    }

    double PhaseFilter::wrapPhase(double phase)
    {
        while (phase > std::numbers::pi)
            phase -= 2 * std::numbers::pi;
        while (phase < -std::numbers::pi)
            phase += 2 * std::numbers::pi;
        return phase;
    }

}  // namespace soilwatercm::infrastructure
