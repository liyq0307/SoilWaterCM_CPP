#include "image_service.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <set>
#include <sstream>

#include "../common/exceptions.h"
#include "../infrastructure/config_manager.h"
#include "../utils/logger.h"
#include "../utils/time_utils.h"

namespace soilwatercm::services
{

    ImageService::ImageService(std::shared_ptr<dao::ImageDao> imageDao,
                               std::shared_ptr<infrastructure::FileManager> fileManager,
                               std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal)
        : imageDao_(std::move(imageDao)), fileManager_(std::move(fileManager)), gdal_(std::move(gdal))
    {
    }

    models::Image ImageService::uploadImage(const ImageUploadParams& params)
    {
        if (!std::filesystem::exists(params.file_path))
        {
            throw common::FileNotFoundException("文件不存在: " + params.file_path);
        }

        std::string imageCode = generateImageCode(params.original_filename);

        std::string storagePath = generateStoragePath(params.project_id, params.period_id, params.original_filename);

        std::filesystem::create_directories(std::filesystem::path(storagePath).parent_path());
        std::filesystem::copy_file(params.file_path, storagePath, std::filesystem::copy_options::overwrite_existing);

        auto fileSize = std::filesystem::file_size(storagePath);

        models::Image image;
        image.image_code = imageCode;
        image.type = params.type;
        image.sensor_type = params.sensor_type.value_or("Unknown");
        image.acquisition_time = std::chrono::system_clock::now();
        image.file_path = storagePath;
        image.file_size = static_cast<long>(fileSize);
        image.quality_status = models::QualityStatus::Pending;
        image.project_id = params.project_id;
        image.period_id = params.period_id;

        int imageId = imageDao_->insert(image);
        image.id = imageId;

        utils::Logger::info("Image uploaded: {} -> {}", imageCode, storagePath);

        return image;
    }

    namespace
    {
        std::string randomHex(int length)
        {
            static const char* hexChars = "0123456789abcdef";
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, 15);
            std::string result;
            result.reserve(length);
            for (int i = 0; i < length; ++i)
                result.push_back(hexChars[dis(gen)]);
            return result;
        }

        std::string sanitizeName(const std::string& name)
        {
            std::string safe = name;
            for (auto& c : safe)
            {
                if (c == '/' || c == '\\')
                    c = '_';
            }
            return safe;
        }

        // source_type 字符串 -> ImageType (对齐 Python 的语义映射)
        models::ImageType imageTypeFromSource(const std::string& sourceType)
        {
            if (sourceType.find("sar") != std::string::npos)
                return models::ImageType::Sar;
            if (sourceType.find("uav") != std::string::npos || sourceType.find("oblique") != std::string::npos)
                return models::ImageType::Uav;
            if (sourceType.find("dem") != std::string::npos)
                return models::ImageType::Dem;
            return models::ImageType::Optical;
        }

        // 解析 ISO 时间 (失败返回当前时间)
        std::chrono::system_clock::time_point parseAcquisitionTime(const std::string& iso)
        {
            if (!iso.empty())
            {
                int64_t ts = utils::TimeUtils::fromIso8601(iso);
                if (ts > 0)
                    return std::chrono::system_clock::from_time_t(ts);
                ts = utils::TimeUtils::parse(iso, "%Y-%m-%d");
                if (ts > 0)
                    return std::chrono::system_clock::from_time_t(ts);
            }
            return std::chrono::system_clock::now();
        }

        std::string formatFromExtension(const std::string& path)
        {
            std::string ext = std::filesystem::path(path).extension().string();
            if (!ext.empty() && ext[0] == '.')
                ext.erase(0, 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::toupper(c); });
            return ext.empty() ? "UNKNOWN" : ext;
        }
    }  // namespace

    std::string ImageService::chunkDirPath(int projectId, const std::string& uploadId) const
    {
        std::filesystem::path base(infrastructure::ConfigManager::getInstance().getStorageRawDir());
        return (base / "uploads" / std::to_string(projectId) / uploadId).string();
    }

    ChunkInitResult ImageService::initChunkUpload(int projectId, const std::string& filename, long long totalSize)
    {
        (void)totalSize;
        std::string safeName = sanitizeName(filename);
        std::string uploadId = randomHex(32) + "_" + safeName;

        std::filesystem::path dir(chunkDirPath(projectId, uploadId));
        std::filesystem::create_directories(dir);

        ChunkInitResult result;
        result.upload_id = uploadId;
        result.chunk_size = 50LL * 1024 * 1024;
        return result;
    }

    void ImageService::saveChunk(int projectId, const std::string& uploadId, int chunkIndex,
                                 const std::string& content)
    {
        std::filesystem::path dir(chunkDirPath(projectId, uploadId));
        std::filesystem::create_directories(dir);

        std::ostringstream name;
        name << "chunk_" << std::setw(6) << std::setfill('0') << chunkIndex;
        std::ofstream out((dir / name.str()).string(), std::ios::binary);
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!out)
        {
            throw common::FileException("分片写入失败: " + uploadId);
        }
    }

    models::Image ImageService::mergeChunks(const std::string& uploadId, const ImageFormParams& params)
    {
        std::filesystem::path dir(chunkDirPath(params.project_id, uploadId));
        if (!std::filesystem::exists(dir))
        {
            throw common::ValidationException("未找到分片数据, 请先调用 /upload/chunk");
        }

        std::vector<std::filesystem::path> chunks;
        for (const auto& entry : std::filesystem::directory_iterator(dir))
        {
            if (entry.is_regular_file() && entry.path().filename().string().rfind("chunk_", 0) == 0)
                chunks.push_back(entry.path());
        }
        std::sort(chunks.begin(), chunks.end());
        if (chunks.empty())
        {
            throw common::ValidationException("未找到分片数据, 请先调用 /upload/chunk");
        }

        // 目标路径: {raw_dir}/project_{id}/{filename} (filename 可含相对路径, 保留目录结构)
        std::filesystem::path base(infrastructure::ConfigManager::getInstance().getStorageRawDir());
        std::filesystem::path saveDir = base / ("project_" + std::to_string(params.project_id));
        std::filesystem::path inputName(params.filename);
        if (inputName.is_absolute())
        {
            throw common::ValidationException("filename 不允许为绝对路径: " + params.filename);
        }
        std::filesystem::path dest = (saveDir / inputName).lexically_normal();
        // 防目录穿越: 归一化后必须仍在 saveDir 内
        auto rel = dest.lexically_relative(saveDir);
        if (rel.empty() || *rel.begin() == "..")
        {
            throw common::ValidationException("非法文件名: " + params.filename);
        }
        std::filesystem::create_directories(dest.parent_path());

        long long total = 0;
        {
            std::ofstream out(dest.string(), std::ios::binary);
            for (const auto& chunk : chunks)
            {
                std::ifstream in(chunk.string(), std::ios::binary);
                out << in.rdbuf();
                total += static_cast<long long>(std::filesystem::file_size(chunk));
            }
        }

        // 清理分片目录
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);

        ImageFormParams mergedParams = params;
        if (mergedParams.file_format.empty())
            mergedParams.file_format = formatFromExtension(dest.string());

        auto image = createImageRecord(mergedParams, dest.string(), static_cast<size_t>(total),
                                       std::filesystem::path(params.filename).filename().string());

        utils::Logger::info("Image chunks merged: {} ({} bytes)", dest.string(), total);
        return image;
    }

    models::Image ImageService::importUploadedFile(const ImageFormParams& params, const std::string& tempPath)
    {
        if (!std::filesystem::exists(tempPath))
        {
            throw common::FileNotFoundException("临时文件不存在: " + tempPath);
        }

        std::filesystem::path base(infrastructure::ConfigManager::getInstance().getStorageRawDir());
        std::filesystem::path saveDir = base / ("project_" + std::to_string(params.project_id));
        std::filesystem::path inputName(params.filename);
        if (inputName.is_absolute())
        {
            throw common::ValidationException("filename 不允许为绝对路径: " + params.filename);
        }
        std::filesystem::path dest = (saveDir / inputName).lexically_normal();
        auto rel = dest.lexically_relative(saveDir);
        if (rel.empty() || *rel.begin() == "..")
        {
            throw common::ValidationException("非法文件名: " + params.filename);
        }
        std::filesystem::create_directories(dest.parent_path());

        std::filesystem::copy_file(tempPath, dest.string(), std::filesystem::copy_options::overwrite_existing);
        auto fileSize = std::filesystem::file_size(dest.string());

        ImageFormParams copyParams = params;
        if (copyParams.file_format.empty())
            copyParams.file_format = formatFromExtension(dest.string());

        return createImageRecord(copyParams, dest.string(), static_cast<size_t>(fileSize),
                                 std::filesystem::path(params.filename).filename().string());
    }

    models::Image ImageService::registerExistingPath(const ImageFormParams& params, const std::string& absolutePath)
    {
        std::filesystem::path p(absolutePath);
        if (!p.is_absolute())
        {
            throw common::ValidationException("file_path 必须为绝对路径");
        }
        std::error_code ec;
        if (!std::filesystem::is_regular_file(p, ec))
        {
            throw common::ValidationException("文件不存在: " + absolutePath);
        }

        size_t fileSize = static_cast<size_t>(std::filesystem::file_size(p, ec));

        ImageFormParams copyParams = params;
        if (copyParams.file_format.empty())
            copyParams.file_format = formatFromExtension(absolutePath);

        auto image = createImageRecord(copyParams, p.string(), fileSize, p.filename().string());
        utils::Logger::info("Image registered by path: {}", p.string());
        return image;
    }

    std::pair<std::vector<int>, std::vector<std::string>> ImageService::registerFolder(const ImageFormParams& params,
                                                                                       const std::string& folderPath)
    {
        std::filesystem::path root(folderPath);
        if (!root.is_absolute())
        {
            throw common::ValidationException("folder_path 必须为绝对路径");
        }
        std::error_code ec;
        if (!std::filesystem::is_directory(root, ec))
        {
            throw common::ValidationException("目录不存在: " + folderPath);
        }

        static const std::set<std::string> imageExts = {".tif", ".tiff", ".img", ".jp2", ".png", ".bmp"};

        std::vector<int> ids;
        std::vector<std::string> skipped;

        std::vector<std::filesystem::path> paths;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec))
        {
            if (entry.is_regular_file())
                paths.push_back(entry.path());
        }
        std::sort(paths.begin(), paths.end());

        for (const auto& path : paths)
        {
            std::string ext = path.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
            if (imageExts.find(ext) == imageExts.end())
                continue;

            std::error_code fec;
            auto size = std::filesystem::file_size(path, fec);
            if (fec)
            {
                skipped.push_back(path.string() + " (读取失败: " + fec.message() + ")");
                continue;
            }

            ImageFormParams copyParams = params;
            copyParams.file_format = formatFromExtension(path.string());
            auto image = createImageRecord(copyParams, path.string(), static_cast<size_t>(size),
                                           path.filename().string());
            ids.push_back(image.id);
        }

        return {ids, skipped};
    }

    models::Image ImageService::registerOblique(const ImageFormParams& params, const std::string& path)
    {
        std::filesystem::path p(path);
        if (!p.is_absolute())
        {
            throw common::ValidationException("file_path 必须为绝对路径");
        }

        std::error_code ec;
        std::filesystem::path tileset;
        if (std::filesystem::is_directory(p, ec))
        {
            auto direct = p / "tileset.json";
            if (std::filesystem::is_regular_file(direct, ec))
            {
                tileset = direct;
            }
            else
            {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(p, ec))
                {
                    if (entry.is_regular_file() && entry.path().filename() == "tileset.json")
                    {
                        tileset = entry.path();
                        break;
                    }
                }
            }
        }
        else if (std::filesystem::is_regular_file(p, ec) && p.filename().string() == "tileset.json")
        {
            tileset = p;
        }

        if (tileset.empty())
        {
            throw common::ValidationException("未找到 tileset.json: " + path);
        }

        ImageFormParams copyParams = params;
        copyParams.source_type = "uav_oblique";
        copyParams.file_format = "3DTiles";

        std::string displayName = tileset.parent_path().filename().string();
        if (displayName.empty())
            displayName = "tileset";

        auto image = createImageRecord(copyParams, tileset.string(), 0, displayName);
        utils::Logger::info("Oblique tileset registered: {}", tileset.string());
        return image;
    }

    bool ImageService::deletePreprocessResults(int imageId)
    {
        auto imageOpt = imageDao_->findById(imageId);
        if (!imageOpt)
        {
            throw common::NotFoundException("影像不存在: " + std::to_string(imageId));
        }

        const auto& image = *imageOpt;
        if (image.file_path.empty())
        {
            return false;
        }

        std::filesystem::path mainPath(image.file_path);
        std::string stem = mainPath.stem().string();
        std::string ext = mainPath.extension().string();
        std::filesystem::path parent = mainPath.parent_path();

        static const std::vector<std::string> suffixes = {"_rad", "_geom", "_ortho", "_resamp"};
        bool removed = false;
        std::error_code ec;
        for (const auto& suffix : suffixes)
        {
            auto product = parent / (stem + suffix + ext);
            if (std::filesystem::is_regular_file(product, ec))
            {
                std::filesystem::remove(product, ec);
                removed = true;
            }
        }

        if (removed)
        {
            utils::Logger::info("Preprocess products deleted for image {}", imageId);
        }
        return removed;
    }

    models::Image ImageService::createImageRecord(const ImageFormParams& params, const std::string& filePath,
                                                  size_t fileSize, const std::string& displayName)
    {
        models::Image image;
        image.image_code = generateImageCode(displayName);
        image.type = imageTypeFromSource(params.source_type);
        image.sensor_type = params.sensor.empty() ? "Unknown" : params.sensor;
        image.acquisition_time = parseAcquisitionTime(params.acquisition_date);
        image.file_path = filePath;
        image.file_size = fileSize;
        image.quality_status = models::QualityStatus::Pending;
        image.project_id = params.project_id;
        image.period_id = params.period_id;
        image.resolution = params.resolution_m.value_or(0.0);

        int imageId = imageDao_->insert(image);
        if (imageId < 0)
        {
            throw common::DatabaseException("影像记录写入失败");
        }
        image.id = imageId;
        return image;
    }

    std::optional<models::Image> ImageService::getImageById(int id)
    {
        return imageDao_->findById(id);
    }

    std::vector<models::Image> ImageService::getImagesByProject(int projectId)
    {
        return imageDao_->findByProjectId(projectId);
    }

    std::vector<models::Image> ImageService::getImagesByPeriod(int periodId)
    {
        return imageDao_->findByPeriodId(periodId);
    }

    bool ImageService::deleteImage(int id)
    {
        auto imageOpt = imageDao_->findById(id);
        if (!imageOpt)
        {
            throw common::NotFoundException("影像不存在: " + std::to_string(id));
        }

        const auto& image = *imageOpt;

        if (!image.file_path.empty())
        {
            try
            {
                std::filesystem::remove(image.file_path);
                utils::Logger::info("Image file deleted: {}", image.file_path);
            }
            catch (const std::exception& e)
            {
                utils::Logger::warn("Failed to delete image file: {}", e.what());
            }
        }

        bool success = imageDao_->remove(id);

        if (success)
        {
            utils::Logger::info("Image deleted: {}", image.image_code);
        }

        return success;
    }

    std::string ImageService::getImageFilePath(int id)
    {
        auto imageOpt = imageDao_->findById(id);
        if (!imageOpt)
        {
            throw common::NotFoundException("影像不存在: " + std::to_string(id));
        }

        return imageOpt->file_path;
    }

    std::future<ImageQualityCheckResult> ImageService::triggerQualityCheck(int imageId)
    {
        return std::async(std::launch::async, [this, imageId]() { return this->performQualityCheck(imageId); });
    }

    ImageQualityCheckResult ImageService::performQualityCheck(int imageId)
    {
        auto imageOpt = imageDao_->findById(imageId);
        if (!imageOpt)
        {
            ImageQualityCheckResult result;
            result.is_valid = false;
            result.message = "影像不存在";
            return result;
        }

        const auto& image = *imageOpt;

        if (image.file_path.empty() || !std::filesystem::exists(image.file_path))
        {
            ImageQualityCheckResult result;
            result.is_valid = false;
            result.message = "影像文件不存在";
            updateQualityStatus(imageId, models::QualityStatus::Invalid, result.message);
            return result;
        }

        ImageQualityCheckResult result = checkImageQuality(image.file_path);

        if (result.is_valid)
        {
            updateQualityStatus(imageId, models::QualityStatus::Valid, result.message);
        }
        else
        {
            updateQualityStatus(imageId, models::QualityStatus::Invalid, result.message);
        }

        utils::Logger::info("Image quality check completed: {} -> {}", image.image_code,
                            result.is_valid ? "Valid" : "Invalid");

        return result;
    }

    bool ImageService::updateQualityStatus(int imageId, models::QualityStatus status, const std::string& message)
    {
        return imageDao_->updateQualityStatus(imageId, status, message);
    }

    std::vector<models::Image> ImageService::findImagesByQualityStatus(models::QualityStatus status)
    {
        return imageDao_->findByQualityStatus(status);
    }

    std::string ImageService::generateImageCode(const std::string& originalFilename)
    {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1000, 9999);

        std::filesystem::path p(originalFilename);
        std::string stem = p.stem().string();

        return "IMG_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen));
    }

    std::string ImageService::generateStoragePath(int projectId, int periodId, const std::string& filename)
    {
        return "data/projects/" + std::to_string(projectId) + "/periods/" + std::to_string(periodId) + "/images/" +
               filename;
    }

    ImageQualityCheckResult ImageService::checkImageQuality(const std::string& filePath)
    {
        ImageQualityCheckResult result;

        try
        {
            auto rasterInfo = gdal_->getRasterInfo(filePath);

            result.is_valid = true;
            result.message = "影像质量检查通过";

            {
                auto& info = rasterInfo;
                models::Geometry extent;
                extent.wkt = "POLYGON((" + std::to_string(info.xMin) + " " + std::to_string(info.yMin) + ", " +
                             std::to_string(info.xMax) + " " + std::to_string(info.yMin) + ", " +
                             std::to_string(info.xMax) + " " + std::to_string(info.yMax) + ", " +
                             std::to_string(info.xMin) + " " + std::to_string(info.yMax) + ", " +
                             std::to_string(info.xMin) + " " + std::to_string(info.yMin) + "))";
                extent.is_valid = true;
                result.extent = extent;
            }
            result.crs = rasterInfo.crs;
            result.resolution = rasterInfo.resolution;

            if (rasterInfo.bandCount >= 1)
            {
                result.cloud_cover = 0.0;
            }
        }
        catch (const std::exception& e)
        {
            result.is_valid = false;
            result.message = std::string("影像质量检查失败: ") + e.what();
        }

        return result;
    }

}  // namespace soilwatercm::services
