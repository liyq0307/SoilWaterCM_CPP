#include "raster_processor.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <gdalwarper.h>

namespace soilwatercm::infrastructure::gdal
{

    RasterProcessor::RasterProcessor() {}

    RasterData RasterProcessor::resample(const RasterData& input, int target_width, int target_height,
                                         const std::string& method)
    {
        RasterData result;
        result.width = target_width;
        result.height = target_height;
        result.bands = input.bands;
        result.data_type = input.data_type;
        result.projection = input.projection;

        for (int i = 0; i < 6; ++i)
        {
            result.geotransform[i] = input.geotransform[i];
        }

        double x_scale = static_cast<double>(input.width) / target_width;
        double y_scale = static_cast<double>(input.height) / target_height;

        result.geotransform[1] *= x_scale;
        result.geotransform[5] *= y_scale;

        result.data.resize(static_cast<size_t>(target_width) * target_height * input.bands);

        for (int band = 0; band < input.bands; ++band)
        {
            size_t band_offset = static_cast<size_t>(band) * input.width * input.height;
            size_t result_offset = static_cast<size_t>(band) * target_width * target_height;

            for (int y = 0; y < target_height; ++y)
            {
                for (int x = 0; x < target_width; ++x)
                {
                    double src_x = x * x_scale;
                    double src_y = y * y_scale;

                    int x0 = static_cast<int>(src_x);
                    int y0 = static_cast<int>(src_y);

                    x0 = std::max(0, std::min(x0, input.width - 1));
                    y0 = std::max(0, std::min(y0, input.height - 1));

                    size_t src_idx = band_offset + static_cast<size_t>(y0) * input.width + x0;
                    size_t dst_idx = result_offset + static_cast<size_t>(y) * target_width + x;

                    result.data[dst_idx] = input.data[src_idx];
                }
            }
        }

        return result;
    }

    RasterData RasterProcessor::clip(const RasterData& input, double x_min, double y_min, double x_max, double y_max)
    {
        return input;
    }

    RasterData RasterProcessor::mosaic(const std::vector<RasterData>& inputs)
    {
        if (inputs.empty())
        {
            return RasterData();
        }
        return inputs[0];
    }

    RasterData RasterProcessor::calculate(const RasterData& input, const std::string& expression)
    {
        RasterData result = input;

        for (auto& value : result.data)
        {
            if (expression == "log")
            {
                value = std::log(value);
            }
            else if (expression == "exp")
            {
                value = std::exp(value);
            }
            else if (expression == "sqrt")
            {
                value = std::sqrt(value);
            }
            else if (expression == "abs")
            {
                value = std::abs(value);
            }
        }

        return result;
    }

    RasterData RasterProcessor::calculate(const RasterData& input1, const RasterData& input2,
                                          const std::string& expression)
    {
        RasterData result = input1;

        if (input1.data.size() != input2.data.size())
        {
            throw std::runtime_error("Input rasters must have the same size");
        }

        for (size_t i = 0; i < result.data.size(); ++i)
        {
            if (expression == "+")
            {
                result.data[i] = input1.data[i] + input2.data[i];
            }
            else if (expression == "-")
            {
                result.data[i] = input1.data[i] - input2.data[i];
            }
            else if (expression == "*")
            {
                result.data[i] = input1.data[i] * input2.data[i];
            }
            else if (expression == "/")
            {
                result.data[i] = input2.data[i] != 0 ? input1.data[i] / input2.data[i] : 0;
            }
        }

        return result;
    }

    RasterData RasterProcessor::normalize(const RasterData& input, double min_val, double max_val)
    {
        RasterData result = input;

        if (input.data.empty())
        {
            return result;
        }

        auto [min_it, max_it] = std::minmax_element(input.data.begin(), input.data.end());
        double data_min = *min_it;
        double data_max = *max_it;

        double range = data_max - data_min;
        if (range == 0)
        {
            return result;
        }

        for (auto& value : result.data)
        {
            value = min_val + (value - data_min) / range * (max_val - min_val);
        }

        return result;
    }

    RasterData RasterProcessor::fillNodata(const RasterData& input, double fill_value)
    {
        RasterData result = input;
        return result;
    }

    std::vector<double> RasterProcessor::getStatistics(const RasterData& input)
    {
        std::vector<double> stats(5);

        if (input.data.empty())
        {
            return stats;
        }

        double sum = 0.0;
        double min_val = input.data[0];
        double max_val = input.data[0];

        for (const auto& value : input.data)
        {
            sum += value;
            min_val = std::min(min_val, value);
            max_val = std::max(max_val, value);
        }

        double mean = sum / input.data.size();

        double variance = 0.0;
        for (const auto& value : input.data)
        {
            variance += (value - mean) * (value - mean);
        }
        variance /= input.data.size();

        stats[0] = min_val;
        stats[1] = max_val;
        stats[2] = mean;
        stats[3] = std::sqrt(variance);
        stats[4] = sum;

        return stats;
    }

    RasterData RasterProcessor::applyFilter(const RasterData& input, const std::string& filter_type, int kernel_size)
    {
        if (input.data.empty())
            return input;

        RasterData result = input;
        int half = kernel_size / 2;

        for (int band = 0; band < input.bands; ++band)
        {
            size_t bandOffset = static_cast<size_t>(band) * input.width * input.height;

            for (int y = half; y < input.height - half; ++y)
            {
                for (int x = half; x < input.width - half; ++x)
                {
                    size_t idx = bandOffset + static_cast<size_t>(y) * input.width + x;

                    if (filter_type == "mean")
                    {
                        double sum = 0.0;
                        int count = 0;
                        for (int dy = -half; dy <= half; ++dy)
                        {
                            for (int dx = -half; dx <= half; ++dx)
                            {
                                size_t nidx = bandOffset + static_cast<size_t>(y + dy) * input.width + (x + dx);
                                sum += input.data[nidx];
                                count++;
                            }
                        }
                        result.data[idx] = sum / count;
                    }
                    else if (filter_type == "median")
                    {
                        std::vector<double> values;
                        for (int dy = -half; dy <= half; ++dy)
                        {
                            for (int dx = -half; dx <= half; ++dx)
                            {
                                size_t nidx = bandOffset + static_cast<size_t>(y + dy) * input.width + (x + dx);
                                values.push_back(input.data[nidx]);
                            }
                        }
                        std::sort(values.begin(), values.end());
                        result.data[idx] = values[values.size() / 2];
                    }
                    else if (filter_type == "gaussian")
                    {
                        double sum = 0.0;
                        double wsum = 0.0;
                        double sigma = kernel_size / 3.0;
                        for (int dy = -half; dy <= half; ++dy)
                        {
                            for (int dx = -half; dx <= half; ++dx)
                            {
                                size_t nidx = bandOffset + static_cast<size_t>(y + dy) * input.width + (x + dx);
                                double w = std::exp(-(dx * dx + dy * dy) / (2 * sigma * sigma));
                                sum += input.data[nidx] * w;
                                wsum += w;
                            }
                        }
                        result.data[idx] = sum / wsum;
                    }
                }
            }
        }

        return result;
    }

    void RasterProcessor::resample(const std::string& inputPath, const std::string& outputPath, double targetResolution,
                                   const std::string& method)
    {
        GDALDataset* srcDs = static_cast<GDALDataset*>(GDALOpen(inputPath.c_str(), GA_ReadOnly));
        if (!srcDs)
        {
            throw std::runtime_error("Failed to open input file: " + inputPath);
        }

        int srcXSize = srcDs->GetRasterXSize();
        int srcYSize = srcDs->GetRasterYSize();

        double gt[6];
        srcDs->GetGeoTransform(gt);

        double srcResX = std::abs(gt[1]);
        double srcResY = std::abs(gt[5]);

        int dstXSize = static_cast<int>(std::round(srcXSize * srcResX / targetResolution));
        int dstYSize = static_cast<int>(std::round(srcYSize * srcResY / targetResolution));

        GDALDataset* dstDs = nullptr;
        GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
        if (driver)
        {
            dstDs =
                driver->Create(outputPath.c_str(), dstXSize, dstYSize, srcDs->GetRasterCount(), GDT_Float64, nullptr);
        }

        if (!dstDs)
        {
            GDALClose(srcDs);
            throw std::runtime_error("Failed to create output file: " + outputPath);
        }

        double dstGt[6] = {gt[0], targetResolution * (gt[1] > 0 ? 1 : -1), gt[2], gt[3],
                           gt[4], -targetResolution * (gt[5] > 0 ? 1 : -1)};
        dstDs->SetGeoTransform(dstGt);
        dstDs->SetProjection(srcDs->GetProjectionRef());

        {
            CPLErr err =
                GDALReprojectImage(GDALDataset::ToHandle(srcDs), nullptr, GDALDataset::ToHandle(dstDs), nullptr,
                                   static_cast<GDALResampleAlg>(GRIORA_Bilinear), 0.0, 0.0, nullptr, nullptr, nullptr);
            if (err != CE_None)
            {
                GDALClose(srcDs);
                GDALClose(dstDs);
                throw std::runtime_error("Reprojection failed");
            }
        }

        GDALClose(srcDs);
        GDALClose(dstDs);
    }

    void RasterProcessor::mosaic(const std::vector<std::string>& inputPaths, const std::string& outputPath,
                                 const std::string& method, double nodataValue)
    {
        if (inputPaths.empty())
        {
            throw std::runtime_error("No input files for mosaicking");
        }

        std::vector<GDALDataset*> datasets;
        for (const auto& path : inputPaths)
        {
            GDALDataset* ds = static_cast<GDALDataset*>(GDALOpen(path.c_str(), GA_ReadOnly));
            if (ds)
            {
                datasets.push_back(ds);
            }
        }

        if (datasets.empty())
        {
            throw std::runtime_error("Failed to open any input files");
        }

        double overallMinX = 1e18, overallMinY = 1e18;
        double overallMaxX = -1e18, overallMaxY = -1e18;
        double resX = 0, resY = 0;

        for (auto* ds : datasets)
        {
            double gt[6];
            ds->GetGeoTransform(gt);
            double minX = gt[0];
            double minY = gt[3] + gt[5] * ds->GetRasterYSize();
            double maxX = gt[0] + gt[1] * ds->GetRasterXSize();
            double maxY = gt[3];

            overallMinX = std::min(overallMinX, minX);
            overallMinY = std::min(overallMinY, minY);
            overallMaxX = std::max(overallMaxX, maxX);
            overallMaxY = std::max(overallMaxY, maxY);
            resX = std::abs(gt[1]);
            resY = std::abs(gt[5]);
        }

        int outXSize = static_cast<int>(std::round((overallMaxX - overallMinX) / resX));
        int outYSize = static_cast<int>(std::round((overallMaxY - overallMinY) / resY));

        GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
        GDALDataset* outDs = driver->Create(outputPath.c_str(), outXSize, outYSize, 1, GDT_Float64, nullptr);

        double outGt[6] = {overallMinX, resX, 0, overallMaxY, 0, -resY};
        outDs->SetGeoTransform(outGt);
        outDs->SetProjection(datasets[0]->GetProjectionRef());

        GDALRasterBand* outBand = outDs->GetRasterBand(1);
        outBand->Fill(nodataValue);

        std::vector<double> outData(outXSize * outYSize, nodataValue);

        for (auto* ds : datasets)
        {
            double gt[6];
            ds->GetGeoTransform(gt);
            int dsXSize = ds->GetRasterXSize();
            int dsYSize = ds->GetRasterYSize();

            int startX = static_cast<int>(std::round((gt[0] - overallMinX) / resX));
            int startY = static_cast<int>(std::round((overallMaxY - (gt[3])) / resY));

            startX = std::max(0, startX);
            startY = std::max(0, startY);

            GDALRasterBand* srcBand = ds->GetRasterBand(1);
            std::vector<double> srcData(dsXSize * dsYSize);
            srcBand->RasterIO(GF_Read, 0, 0, dsXSize, dsYSize, srcData.data(), dsXSize, dsYSize, GDT_Float64, 0, 0);

            for (int y = 0; y < dsYSize && (startY + y) < outYSize; ++y)
            {
                for (int x = 0; x < dsXSize && (startX + x) < outXSize; ++x)
                {
                    size_t srcIdx = static_cast<size_t>(y) * dsXSize + x;
                    size_t dstIdx = static_cast<size_t>(startY + y) * outXSize + (startX + x);
                    double val = srcData[srcIdx];

                    if (val == nodataValue)
                        continue;

                    if (method == "first" && outData[dstIdx] == nodataValue)
                    {
                        outData[dstIdx] = val;
                    }
                    else if (method == "last")
                    {
                        outData[dstIdx] = val;
                    }
                    else if (method == "max")
                    {
                        if (outData[dstIdx] == nodataValue || val > outData[dstIdx])
                        {
                            outData[dstIdx] = val;
                        }
                    }
                    else if (method == "min")
                    {
                        if (outData[dstIdx] == nodataValue || val < outData[dstIdx])
                        {
                            outData[dstIdx] = val;
                        }
                    }
                    else if (method == "mean")
                    {
                        if (outData[dstIdx] == nodataValue)
                        {
                            outData[dstIdx] = val;
                        }
                        else
                        {
                            outData[dstIdx] = (outData[dstIdx] + val) / 2.0;
                        }
                    }
                    else
                    {
                        if (outData[dstIdx] == nodataValue)
                        {
                            outData[dstIdx] = val;
                        }
                    }
                }
            }
        }

        outBand->RasterIO(GF_Write, 0, 0, outXSize, outYSize, outData.data(), outXSize, outYSize, GDT_Float64, 0, 0);

        for (auto* ds : datasets)
        {
            GDALClose(ds);
        }
        GDALClose(outDs);
    }

}  // namespace soilwatercm::infrastructure::gdal
