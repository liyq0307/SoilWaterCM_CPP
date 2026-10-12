#include "interferogram.h"

#include <cmath>
#include <filesystem>
#include <future>
#include <mutex>
#include <thread>

#include "utils/logger.h"

namespace soilwatercm::infrastructure
{

    InterferogramGenerator::InterferogramGenerator()
        : logger_(utils::Logger::getLogger("interferogram")), num_threads_(std::thread::hardware_concurrency())
    {
        logger_->info("Interferogram generator initialized, threads: {}", num_threads_);
    }

    InterferogramResult InterferogramGenerator::generate(const std::vector<std::complex<float>>& masterImage,
                                                         const std::vector<std::complex<float>>& slaveImage, int width,
                                                         int height)
    {
        InterferogramResult result;

        if (masterImage.size() != slaveImage.size())
        {
            result.success = false;
            result.errorMessage = "Master and slave image sizes mismatch";
            return result;
        }

        size_t totalPixels = width * height;
        if (masterImage.size() != totalPixels)
        {
            result.success = false;
            result.errorMessage = "Image size does not match width * height";
            return result;
        }

        logger_->info("Generating interferogram, size: {}x{}, pixels: {}", width, height, totalPixels);

        result.phase.width = width;
        result.phase.height = height;
        result.phase.data.resize(totalPixels);

        result.amplitude.width = width;
        result.amplitude.height = height;
        result.amplitude.data.resize(totalPixels);

        int numThreads = std::min(num_threads_, static_cast<int>(totalPixels / 1000));
        if (numThreads < 1)
            numThreads = 1;

        std::vector<std::thread> threads;
        std::mutex mutex;
        std::atomic<int> pixelsProcessed{0};

        auto processChunk = [&](int startIdx, int endIdx)
        {
            for (int i = startIdx; i < endIdx; ++i)
            {
                std::complex<float> master = masterImage[i];
                std::complex<float> slave = slaveImage[i];

                std::complex<float> interferogram = master * conjugate(slave);

                result.phase.data[i] = phase(interferogram);
                result.amplitude.data[i] = amplitude(interferogram);

                pixelsProcessed++;
            }
        };

        int chunkSize = static_cast<int>(totalPixels / numThreads);
        for (int t = 0; t < numThreads; ++t)
        {
            int startIdx = t * chunkSize;
            int endIdx = (t == numThreads - 1) ? static_cast<int>(totalPixels) : (t + 1) * chunkSize;
            threads.emplace_back(processChunk, startIdx, endIdx);
        }

        for (auto& thread : threads)
        {
            thread.join();
        }

        result.coherence = calculateCoherence(masterImage, slaveImage, width, height);

        result.success = true;
        logger_->info("Interferogram generation completed");

        return result;
    }

    InterferogramResult InterferogramGenerator::generateFromFiles(const std::string& masterFile,
                                                                  const std::string& slaveFile,
                                                                  const std::string& outputDir)
    {
        logger_->info("Generating interferogram from files: {}, {}", masterFile, slaveFile);

        std::vector<std::complex<float>> masterImage = readComplexImage(masterFile);
        std::vector<std::complex<float>> slaveImage = readComplexImage(slaveFile);

        if (masterImage.empty() || slaveImage.empty())
        {
            InterferogramResult result;
            result.success = false;
            result.errorMessage = "Failed to read input images";
            return result;
        }

        GdalWrapper gdal;
        RasterData masterMeta = gdal.readRaster(masterFile);

        InterferogramResult result = generate(masterImage, slaveImage, masterMeta.width, masterMeta.height);

        if (result.success)
        {
            namespace fs = std::filesystem;
            fs::path outputDirPath(outputDir);
            fs::create_directories(outputDirPath);

            std::string phaseFile = (outputDirPath / "phase.tif").string();
            std::string ampFile = (outputDirPath / "amplitude.tif").string();
            std::string cohFile = (outputDirPath / "coherence.tif").string();

            gdal.writeRaster(phaseFile, result.phase);
            gdal.writeRaster(ampFile, result.amplitude);
            gdal.writeRaster(cohFile, result.coherence);
        }

        return result;
    }

    std::future<InterferogramResult> InterferogramGenerator::generateAsync(
        const std::vector<std::complex<float>>& masterImage, const std::vector<std::complex<float>>& slaveImage,
        int width, int height)
    {
        return std::async(std::launch::async, [this, masterImage, slaveImage, width, height]()
                          { return generate(masterImage, slaveImage, width, height); });
    }

    RasterData InterferogramGenerator::calculateCoherence(const std::vector<std::complex<float>>& masterImage,
                                                          const std::vector<std::complex<float>>& slaveImage, int width,
                                                          int height, int windowSize)
    {
        RasterData coherence;
        coherence.width = width;
        coherence.height = height;
        coherence.data.resize(width * height);

        int halfWindow = windowSize / 2;

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                std::complex<double> sumInterferogram(0.0, 0.0);
                double sumMasterAmp2 = 0.0;
                double sumSlaveAmp2 = 0.0;
                int count = 0;

                for (int dy = -halfWindow; dy <= halfWindow; ++dy)
                {
                    for (int dx = -halfWindow; dx <= halfWindow; ++dx)
                    {
                        int ny = y + dy;
                        int nx = x + dx;

                        if (ny >= 0 && ny < height && nx >= 0 && nx < width)
                        {
                            int idx = ny * width + nx;

                            std::complex<float> master = masterImage[idx];
                            std::complex<float> slave = slaveImage[idx];

                            sumInterferogram += static_cast<std::complex<double>>(master * conjugate(slave));
                            sumMasterAmp2 += static_cast<double>(std::norm(master));
                            sumSlaveAmp2 += static_cast<double>(std::norm(slave));

                            count++;
                        }
                    }
                }

                if (count > 0 && sumMasterAmp2 > 0 && sumSlaveAmp2 > 0)
                {
                    double cohValue = std::abs(sumInterferogram) / std::sqrt(sumMasterAmp2 * sumSlaveAmp2);
                    coherence.data[y * width + x] = static_cast<float>(cohValue);
                }
                else
                {
                    coherence.data[y * width + x] = 0.0f;
                }
            }
        }

        return coherence;
    }

    std::vector<std::complex<float>> InterferogramGenerator::readComplexImage(const std::string& filename)
    {
        std::vector<std::complex<float>> image;

        GdalWrapper gdal;
        RasterData realPart = gdal.readRaster(filename, 1);
        RasterData imagPart = gdal.readRaster(filename, 2);

        if (realPart.data.empty())
        {
            logger_->error("Failed to read complex image: {}", filename);
            return image;
        }

        size_t totalPixels = realPart.width * realPart.height;
        image.resize(totalPixels);

        if (imagPart.data.empty())
        {
            for (size_t i = 0; i < totalPixels; ++i)
            {
                float phase = static_cast<float>(realPart.data[i]);
                image[i] = std::complex<float>(std::cos(phase), std::sin(phase));
            }
        }
        else
        {
            for (size_t i = 0; i < totalPixels; ++i)
            {
                image[i] =
                    std::complex<float>(static_cast<float>(realPart.data[i]), static_cast<float>(imagPart.data[i]));
            }
        }

        return image;
    }

    bool InterferogramGenerator::writeComplexImage(const std::string& filename,
                                                   const std::vector<std::complex<float>>& image, int width, int height)
    {
        std::vector<double> realPart(width * height);
        std::vector<double> imagPart(width * height);

        for (size_t i = 0; i < image.size(); ++i)
        {
            realPart[i] = image[i].real();
            imagPart[i] = image[i].imag();
        }

        GdalWrapper gdal;
        RasterData realData;
        realData.width = width;
        realData.height = height;
        realData.data = realPart;

        return gdal.writeRaster(filename, realData);
    }

}  // namespace soilwatercm::infrastructure
