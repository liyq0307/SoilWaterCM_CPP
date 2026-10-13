#include "three_d_service.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "../common/exceptions.h"
#include "../utils/logger.h"

namespace soilwatercm::services
{

    ThreeDService::ThreeDService(std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal,
                                 std::shared_ptr<infrastructure::external::OdmClient> odm)
        : gdal_(std::move(gdal)), odm_(std::move(odm))
    {
    }

    std::string ThreeDService::convertTo3DTiles(const ThreeDTilesParams& params)
    {
        if (!std::filesystem::exists(params.input_path))
        {
            throw common::FileNotFoundException("Input file not found: " + params.input_path);
        }

        std::filesystem::path outputDir = std::filesystem::path(params.output_path);
        std::filesystem::create_directories(outputDir);

        std::filesystem::path tilesetPath = outputDir / "tileset.json";
        std::string inputExt = std::filesystem::path(params.input_path).extension().string();

        if (inputExt == ".obj" || inputExt == ".ply" || inputExt == ".osgb")
        {
            if (odm_ && odm_->isAvailable())
            {
                utils::Logger::info("Converting 3D model via ODM: {}", params.input_path);
                bool success = odm_->convertTo3DTiles(params.input_path, params.output_path);
                if (success)
                {
                    return params.output_path;
                }
            }

            utils::Logger::info("Creating 3D Tiles tileset.json for: {}", params.input_path);
            std::ofstream ofs(tilesetPath);
            ofs << R"({
  "asset": {
    "version": "1.0",
    "tilesetVersion": "1.0.0"
  },
  "geometricError": )"
                << params.max_error * 1000.0 << R"(,
  "root": {
    "boundingVolume": {
      "box": [0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1]
    },
    "geometricError": 0,
    "content": {
      "uri": "model.obj"
    }
  }
})";
            ofs.close();

            return params.output_path;
        }

        if (inputExt == ".tif" || inputExt == ".tiff")
        {
            auto rasterInfo = gdal_->getRasterInfo(params.input_path);

            std::ofstream ofs(tilesetPath);
            ofs << R"({
  "asset": {
    "version": "1.0"
  },
  "geometricError": 500.0,
  "root": {
    "boundingVolume": {
      "box": [)" << (rasterInfo.xMin + rasterInfo.xMax) / 2.0
                << ", " << (rasterInfo.yMin + rasterInfo.yMax) / 2.0 << ", 0, "
                << (rasterInfo.xMax - rasterInfo.xMin) / 2.0 << ", 0, 0, 0, "
                << (rasterInfo.yMax - rasterInfo.yMin) / 2.0 << R"(, 0, 0, 1000]
    },
    "geometricError": 0,
    "content": {
      "uri": "dem_terrain tileset.json"
    }
  }
})";
            ofs.close();
        }

        return params.output_path;
    }

    double ThreeDService::calculateEarthwork(const EarthworkParams& params)
    {
        auto demBefore = gdal_->readRaster(params.dem_before_path);
        auto demAfter = gdal_->readRaster(params.dem_after_path);

        if (demBefore.width != demAfter.width || demBefore.height != demAfter.height)
        {
            throw common::ValidationException("DEM dimensions do not match");
        }

        double cellArea = std::abs(demBefore.geotransform[1]) * std::abs(demBefore.geotransform[5]);

        double cutVolume = 0.0;
        double fillVolume = 0.0;

        for (size_t i = 0; i < demBefore.data.size(); ++i)
        {
            double diff = demAfter.data[i] - demBefore.data[i];

            if (std::isnan(demBefore.data[i]) || std::isnan(demAfter.data[i]))
            {
                continue;
            }

            double vol = diff * cellArea;
            if (vol > 0)
            {
                cutVolume += vol;
            }
            else
            {
                fillVolume += std::abs(vol);
            }
        }

        utils::Logger::info("Earthwork calculated: cut={}, fill={}, net={}", cutVolume, fillVolume,
                            cutVolume - fillVolume);

        return cutVolume - fillVolume;
    }

    std::string ThreeDService::analyzeInundation(const InundationParams& params, InundationStats* stats)
    {
        auto dem = gdal_->readRaster(params.dem_path);

        std::vector<double> inundationData(dem.data.size());
        std::vector<double> depthData(dem.data.size(), 0.0);

        double totalArea = 0.0;
        double inundatedArea = 0.0;
        double cellArea = std::abs(dem.geotransform[1]) * std::abs(dem.geotransform[5]);

        double depthSum = 0.0;
        double depthMax = 0.0;
        int floodCount = 0;

        for (size_t i = 0; i < dem.data.size(); ++i)
        {
            if (std::isnan(dem.data[i]))
            {
                inundationData[i] = 0.0;
                continue;
            }

            if (dem.data[i] < params.water_level)
            {
                inundationData[i] = 1.0;
                double d = params.water_level - dem.data[i];
                depthData[i] = d;
                inundatedArea += cellArea;
                depthSum += d;
                depthMax = std::max(depthMax, d);
                ++floodCount;
            }
            else
            {
                inundationData[i] = 0.0;
            }
            totalArea += cellArea;
        }

        std::string outputPath = params.dem_path + "_inundation.tif";

        infrastructure::gdal::RasterData outputData;
        outputData.data = inundationData;
        outputData.width = dem.width;
        outputData.height = dem.height;
        outputData.geotransform[0] = dem.geotransform[0];
        outputData.geotransform[1] = dem.geotransform[1];
        outputData.geotransform[2] = dem.geotransform[2];
        outputData.geotransform[3] = dem.geotransform[3];
        outputData.geotransform[4] = dem.geotransform[4];
        outputData.geotransform[5] = dem.geotransform[5];
        outputData.projection = dem.projection;
        outputData.bands = 1;

        gdal_->writeRaster(outputPath, outputData);

        std::string depthPath = params.dem_path + "_depth.tif";
        infrastructure::gdal::RasterData depthOutput;
        depthOutput.data = depthData;
        depthOutput.width = dem.width;
        depthOutput.height = dem.height;
        depthOutput.geotransform[0] = dem.geotransform[0];
        depthOutput.geotransform[1] = dem.geotransform[1];
        depthOutput.geotransform[2] = dem.geotransform[2];
        depthOutput.geotransform[3] = dem.geotransform[3];
        depthOutput.geotransform[4] = dem.geotransform[4];
        depthOutput.geotransform[5] = dem.geotransform[5];
        depthOutput.projection = dem.projection;
        depthOutput.bands = 1;

        gdal_->writeRaster(depthPath, depthOutput);

        if (stats)
        {
            stats->total_area_m2 = totalArea;
            stats->inundated_area_m2 = inundatedArea;
            stats->depth_mean_m = floodCount > 0 ? depthSum / floodCount : 0.0;
            stats->depth_max_m = depthMax;
            stats->mask_path = outputPath;
            stats->depth_path = depthPath;
        }

        utils::Logger::info("Inundation analysis completed: total={}, inundated={}, ratio={}", totalArea, inundatedArea,
                            totalArea > 0 ? inundatedArea / totalArea : 0.0);

        return outputPath;
    }

    std::string ThreeDService::analyzeViewshed(const ViewshedParams& params, ViewshedStats* stats)
    {
        auto dem = gdal_->readRaster(params.dem_path);

        double cellSize = std::abs(dem.geotransform[1]);
        double gt[6];
        gt[0] = dem.geotransform[0];
        gt[1] = dem.geotransform[1];
        gt[2] = dem.geotransform[2];
        gt[3] = dem.geotransform[3];
        gt[4] = dem.geotransform[4];
        gt[5] = dem.geotransform[5];

        double observerGeoX = params.observer_x;
        double observerGeoY = params.observer_y;

        double observerCol = (observerGeoX - gt[0]) / gt[1];
        double observerRow = (observerGeoY - gt[3]) / gt[5];

        int obsCol = static_cast<int>(std::round(observerCol));
        int obsRow = static_cast<int>(std::round(observerRow));

        obsCol = std::max(0, std::min(obsCol, dem.width - 1));
        obsRow = std::max(0, std::min(obsRow, dem.height - 1));

        double observerElev = dem.data[obsRow * dem.width + obsCol] + params.observer_height;

        int maxCells = static_cast<int>(std::round(params.max_distance / cellSize));

        std::vector<double> viewshedData(dem.data.size(), 0.0);

        auto isVisible = [&](int row, int col) -> bool
        {
            double dx = col - observerCol;
            double dy = row - observerRow;
            double dist = std::sqrt(dx * dx + dy * dy) * cellSize;
            if (dist > params.max_distance || dist < cellSize * 0.5)
            {
                return false;
            }

            int steps = static_cast<int>(std::round(dist / cellSize));
            if (steps < 1)
                steps = 1;

            double maxAngle = -1e10;

            for (int step = 1; step <= steps; ++step)
            {
                double t = static_cast<double>(step) / steps;
                int checkCol = obsCol + static_cast<int>(std::round(dx * t));
                int checkRow = obsRow + static_cast<int>(std::round(dy * t));

                if (checkCol < 0 || checkCol >= dem.width || checkRow < 0 || checkRow >= dem.height)
                {
                    return false;
                }

                double checkDist = std::sqrt(std::pow((checkCol - observerCol) * cellSize, 2) +
                                             std::pow((checkRow - observerRow) * cellSize, 2));

                if (checkDist < cellSize * 0.5)
                    continue;

                double checkElev = dem.data[checkRow * dem.width + checkCol];
                double angle = (checkElev - observerElev) / checkDist;

                if (angle > maxAngle)
                {
                    maxAngle = angle;
                }
            }

            double targetElev = dem.data[row * dem.width + col];
            double targetAngle = (targetElev - observerElev) / dist;

            return targetAngle >= maxAngle;
        };

        int radius = maxCells;
        int startRow = std::max(0, obsRow - radius);
        int endRow = std::min(dem.height - 1, obsRow + radius);
        int startCol = std::max(0, obsCol - radius);
        int endCol = std::min(dem.width - 1, obsCol + radius);

        double cellArea = std::abs(dem.geotransform[1]) * std::abs(dem.geotransform[5]);
        double totalArea = 0.0;
        double visibleArea = 0.0;

        for (int row = startRow; row <= endRow; ++row)
        {
            for (int col = startCol; col <= endCol; ++col)
            {
                totalArea += cellArea;
                if (isVisible(row, col))
                {
                    viewshedData[row * dem.width + col] = 1.0;
                    visibleArea += cellArea;
                }
            }
        }

        std::string outputPath = params.dem_path + "_viewshed.tif";

        infrastructure::gdal::RasterData outputData;
        outputData.data = viewshedData;
        outputData.width = dem.width;
        outputData.height = dem.height;
        outputData.geotransform[0] = gt[0];
        outputData.geotransform[1] = gt[1];
        outputData.geotransform[2] = gt[2];
        outputData.geotransform[3] = gt[3];
        outputData.geotransform[4] = gt[4];
        outputData.geotransform[5] = gt[5];
        outputData.projection = dem.projection;
        outputData.bands = 1;

        gdal_->writeRaster(outputPath, outputData);

        if (stats)
        {
            stats->total_area_m2 = totalArea;
            stats->visible_area_m2 = visibleArea;
            stats->visible_ratio = totalArea > 0 ? visibleArea / totalArea : 0.0;
            stats->mask_path = outputPath;
        }

        utils::Logger::info("Viewshed analysis completed: output={}", outputPath);

        return outputPath;
    }

}  // namespace soilwatercm::services
