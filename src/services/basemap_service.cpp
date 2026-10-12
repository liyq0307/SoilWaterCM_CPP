#include "basemap_service.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <sstream>

#include "../common/exceptions.h"
#include "../utils/logger.h"

namespace soilwatercm::services
{

    namespace
    {
        const std::string kBasemapDir = "data/basemaps";

        std::string sanitizeFilename(const std::string& raw)
        {
            std::filesystem::path p(raw);
            std::string name = p.filename().string();
            if (name.empty() || name == "." || name == "..")
                return "";
            return name;
        }

        std::string makePolygonWkt(double minx, double miny, double maxx, double maxy)
        {
            std::ostringstream oss;
            oss << std::setprecision(15);
            oss << "POLYGON((" << minx << " " << miny << ", " << maxx << " " << miny << ", " << maxx << " " << maxy
                << ", " << minx << " " << maxy << ", " << minx << " " << miny << "))";
            return oss.str();
        }

        // numpy 默认线性插值百分位 (valid 已升序)
        double percentile(const std::vector<double>& valid, double p)
        {
            if (valid.empty())
                return 0.0;
            if (valid.size() == 1)
                return valid[0];
            double idx = (p / 100.0) * (static_cast<double>(valid.size()) - 1.0);
            size_t lo = static_cast<size_t>(std::floor(idx));
            size_t hi = static_cast<size_t>(std::ceil(idx));
            if (lo == hi)
                return valid[lo];
            double frac = idx - static_cast<double>(lo);
            return valid[lo] + (valid[hi] - valid[lo]) * frac;
        }
    }  // namespace

    BasemapService::BasemapService(std::shared_ptr<dao::BasemapDao> basemapDao,
                                   std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal)
        : basemapDao_(std::move(basemapDao)), gdal_(std::move(gdal))
    {
    }

    std::vector<models::BasemapImage> BasemapService::list(int projectId)
    {
        return basemapDao_->findByProject(projectId);
    }

    std::optional<models::BasemapImage> BasemapService::getById(int id)
    {
        return basemapDao_->findById(id);
    }

    models::BasemapImage BasemapService::upload(const BasemapUploadParams& params)
    {
        std::string safeName = sanitizeFilename(params.original_filename);
        if (safeName.empty())
            throw common::ValidationException("文件名为空");

        if (!std::filesystem::exists(params.temp_file_path))
            throw common::FileNotFoundException("上传文件不存在: " + params.temp_file_path);

        // 落盘到存储目录
        std::filesystem::create_directories(kBasemapDir);
        std::string storagePath = kBasemapDir + "/" + safeName;
        std::error_code ec;
        std::filesystem::copy_file(params.temp_file_path, storagePath,
                                   std::filesystem::copy_options::overwrite_existing, ec);
        if (ec)
            throw common::FileException("保存底图文件失败: " + storagePath + " - " + ec.message());

        std::string tmpCopy = params.temp_file_path;
        std::error_code rmEc;
        std::filesystem::remove(tmpCopy, rmEc);

        models::BasemapImage rec;
        rec.project_id = params.project_id;
        rec.name = params.name.empty() ? safeName : params.name;
        rec.file_path = storagePath;
        rec.crs = "EPSG:4326";
        rec.note = params.note;
        rec.created_at = std::chrono::system_clock::now();

        std::uintmax_t fileSize = std::filesystem::file_size(storagePath, ec);
        if (!ec)
            rec.file_size_mb = static_cast<double>(fileSize) / 1024.0 / 1024.0;

        // GDAL 读取栅格元数据 (尺寸/波段/CRS/footprint/拉伸), 仅对 GeoTIFF 尝试
        std::string ext = std::filesystem::path(safeName).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
        if (ext == ".tif" || ext == ".tiff")
        {
            try
            {
                infrastructure::gdal::RasterInfo info = gdal_->getRasterInfo(storagePath);
                if (info.width > 0 && info.height > 0)
                {
                    rec.width_px = info.width;
                    rec.height_px = info.height;
                    rec.band_count = info.bandCount;
                }

                GDALDataset* dataset = static_cast<GDALDataset*>(GDALOpen(storagePath.c_str(), GA_ReadOnly));
                if (dataset)
                {
                    std::unique_ptr<GDALDataset, decltype(&GDALClose)> guard(dataset, GDALClose);

                    // CRS: 优先 EPSG 权威代码
                    std::string crsName;
                    OGRSpatialReference srcSrs;
                    const char* proj = dataset->GetProjectionRef();
                    bool hasSrs = proj && *proj && srcSrs.importFromWkt(proj) == OGRERR_NONE;
                    if (hasSrs)
                    {
                        srcSrs.AutoIdentifyEPSG();
                        const char* authCode = srcSrs.GetAuthorityCode(nullptr);
                        const char* authName = srcSrs.GetAuthorityName(nullptr);
                        if (authCode && *authCode)
                            crsName = std::string(authName && *authName ? authName : "EPSG") + ":" + authCode;
                    }
                    if (!crsName.empty() && crsName.size() <= 32)
                        rec.crs = crsName;

                    // footprint: 栅格范围转换到 EPSG:4326
                    double gt[6];
                    if (dataset->GetGeoTransform(gt) == CE_None && (gt[1] != 0.0 || gt[5] != 0.0))
                    {
                        int w = dataset->GetRasterXSize();
                        int h = dataset->GetRasterYSize();
                        // 四角 (仿射变换, 含旋转项 gt[2]/gt[4]): (0,0) (w,0) (w,h) (0,h)
                        double cornersX[4] = {gt[0], gt[0] + gt[1] * w, gt[0] + gt[1] * w + gt[2] * h,
                                              gt[0] + gt[2] * h};
                        double cornersY[4] = {gt[3], gt[3] + gt[4] * w, gt[3] + gt[4] * w + gt[5] * h,
                                              gt[3] + gt[5] * h};

                        std::vector<double> tx, ty;
                        bool needTransform = hasSrs && crsName != "EPSG:4326";
                        OGRCoordinateTransformation* transform = nullptr;
                        OGRSpatialReference dstSrs;
                        if (needTransform && dstSrs.importFromEPSG(4326) == OGRERR_NONE)
                        {
                            srcSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                            dstSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                            transform = OGRCreateCoordinateTransformation(&srcSrs, &dstSrs);
                            if (!transform)
                                needTransform = false;
                        }

                        for (int i = 0; i < 4; ++i)
                        {
                            double x = cornersX[i];
                            double y = cornersY[i];
                            if (transform && !transform->Transform(1, &x, &y))
                            {
                                x = cornersX[i];
                                y = cornersY[i];
                            }
                            tx.push_back(x);
                            ty.push_back(y);
                        }
                        if (transform)
                            OGRCoordinateTransformation::DestroyCT(transform);

                        if (!tx.empty())
                        {
                            double minx = *std::min_element(tx.begin(), tx.end());
                            double maxx = *std::max_element(tx.begin(), tx.end());
                            double miny = *std::min_element(ty.begin(), ty.end());
                            double maxy = *std::max_element(ty.begin(), ty.end());
                            if (maxx > minx && maxy > miny)
                            {
                                rec.footprint.wkt = makePolygonWkt(minx, miny, maxx, maxy);
                                rec.footprint.srid = "EPSG:4326";
                                rec.footprint.is_valid = true;
                            }
                        }
                    }

                    // 2%/98% 百分位拉伸: 抽样第一波段, 最多 2048x2048 像素
                    if (dataset->GetRasterCount() >= 1)
                    {
                        GDALRasterBand* band = dataset->GetRasterBand(1);
                        int outW = std::min(dataset->GetRasterXSize(), 2048);
                        int outH = std::min(dataset->GetRasterYSize(), 2048);
                        std::vector<double> buf(static_cast<size_t>(outW) * outH);
                        CPLErr err = band->RasterIO(GF_Read, 0, 0, dataset->GetRasterXSize(), dataset->GetRasterYSize(),
                                                    buf.data(), outW, outH, GDT_Float64, 0, 0);
                        if (err == CE_None)
                        {
                            int hasNoData = FALSE;
                            double nodata = band->GetNoDataValue(&hasNoData);
                            std::vector<double> valid;
                            valid.reserve(buf.size());
                            for (double v : buf)
                            {
                                if (std::isnan(v) || std::isinf(v))
                                    continue;
                                if (hasNoData && v == nodata)
                                    continue;
                                valid.push_back(v);
                            }
                            if (!valid.empty())
                            {
                                std::sort(valid.begin(), valid.end());
                                double smin = percentile(valid, 2.0);
                                double smax = percentile(valid, 98.0);
                                if (smax <= smin)
                                    smax = smin + 1.0;
                                rec.stretch_min = smin;
                                rec.stretch_max = smax;
                            }
                        }
                    }
                }
            }
            catch (const std::exception& e)
            {
                utils::Logger::warn("底图元数据提取失败 {}: {}", storagePath, e.what());
            }
        }

        int id = basemapDao_->insert(rec);
        if (id < 0)
            throw common::DatabaseException("创建底图记录失败");
        rec.id = id;

        // WMS 图层名 (对齐 Python basemap_layer_name: "basemap_" + id)
        rec.wms_layer_name = "basemap_" + std::to_string(id);
        basemapDao_->update(rec);

        utils::Logger::info("底图已上传: id={} name={} path={}", id, rec.name, storagePath);
        return rec;
    }

    bool BasemapService::remove(int id)
    {
        auto existing = basemapDao_->findById(id);
        if (!existing)
            return false;

        bool ok = basemapDao_->remove(id);
        if (!ok)
            return false;

        // 尽力删除物理文件及附属文件 (stem.*), 失败不阻塞
        const std::string& filePath = existing->file_path;
        if (!filePath.empty())
        {
            try
            {
                std::error_code ec;
                std::filesystem::remove(filePath, ec);
                std::filesystem::path p(filePath);
                std::string stem = p.stem().string();
                auto parent = p.parent_path();
                if (!stem.empty() && std::filesystem::exists(parent, ec))
                {
                    for (const auto& entry : std::filesystem::directory_iterator(parent))
                    {
                        std::error_code fe;
                        if (!entry.is_regular_file(fe) || fe)
                            continue;
                        const std::string name = entry.path().filename().string();
                        if (name.size() > stem.size() + 1 && name.compare(0, stem.size(), stem) == 0 &&
                            name[stem.size()] == '.')
                        {
                            std::error_code rmEc;
                            std::filesystem::remove(entry.path(), rmEc);
                            if (rmEc)
                                utils::Logger::warn("删除底图附属文件失败 {}: {}", entry.path().string(),
                                                    rmEc.message());
                        }
                    }
                }
            }
            catch (const std::exception& e)
            {
                utils::Logger::warn("删除底图文件失败: {}", e.what());
            }
        }

        utils::Logger::info("底图已删除: id={}", id);
        return true;
    }

}  // namespace soilwatercm::services
