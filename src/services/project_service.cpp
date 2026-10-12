#include "project_service.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "../common/exceptions.h"
#include "../infrastructure/config_manager.h"
#include "../utils/geojson_utils.h"
#include "../utils/logger.h"

namespace soilwatercm::services
{

    ProjectService::ProjectService(std::shared_ptr<dao::ProjectDao> projectDao,
                                   std::shared_ptr<dao::MonitoringPeriodDao> periodDao,
                                   std::shared_ptr<infrastructure::gdal::GdalWrapper> gdal,
                                   std::shared_ptr<dao::ProjectDocumentDao> documentDao)
        : projectDao_(std::move(projectDao)), periodDao_(std::move(periodDao)), gdal_(std::move(gdal)),
          documentDao_(std::move(documentDao))
    {
    }

    std::shared_ptr<dao::ProjectDocumentDao> ProjectService::documentDao()
    {
        if (!documentDao_)
        {
            // main.cpp 未注入资料 DAO 时, 按全局配置惰性构建 (进程内共享一个连接池)
            static std::shared_ptr<repository::PostgresRepository> sharedRepo = []()
            {
                auto& config = infrastructure::ConfigManager::getInstance();
                repository::PostgresConfig pgCfg;
                pgCfg.host = config.getPostgresHost();
                pgCfg.port = config.getPostgresPort();
                pgCfg.database = config.getPostgresDatabase();
                pgCfg.user = config.getPostgresUser();
                pgCfg.password = config.getPostgresPassword();
                auto pool = std::make_shared<repository::PostgresConnectionPool>(pgCfg);
                return std::make_shared<repository::PostgresRepository>(pool);
            }();
            documentDao_ = std::make_shared<dao::ProjectDocumentDao>(sharedRepo);
        }
        return documentDao_;
    }

    models::Project ProjectService::createProject(const ProjectCreateParams& params)
    {
        if (projectDao_->existsByCode(params.code))
        {
            throw common::ValidationException("项目编码已存在: " + params.code);
        }

        models::Project project;
        project.code = params.code;
        project.name = params.name;
        project.status = params.status;
        project.created_at = std::chrono::system_clock::now();
        project.updated_at = project.created_at;

        if (params.boundary)
        {
            if (!validateGeometry(*params.boundary))
            {
                throw common::ValidationException("项目边界几何无效");
            }

            auto validGeom = ensureMultiPolygon(*params.boundary);
            validGeom = ensure3D(validGeom);
            project.responsibility_area = validGeom;

            auto centroid = calculateCentroid(validGeom);
            if (centroid)
            {
                project.center_lon = params.center_lon.value_or(centroid->first);
                project.center_lat = params.center_lat.value_or(centroid->second);
            }
        }
        else
        {
            project.center_lon = params.center_lon.value_or(0.0);
            project.center_lat = params.center_lat.value_or(0.0);
        }

        int projectId = projectDao_->insert(project);
        project.id = projectId;

        utils::Logger::info("Project created: {} ({})", project.code, project.name);

        return project;
    }

    std::optional<models::Project> ProjectService::getProjectById(int id)
    {
        return projectDao_->findById(id);
    }

    std::optional<models::Project> ProjectService::getProjectByCode(const std::string& code)
    {
        return projectDao_->findByCode(code);
    }

    std::vector<models::Project> ProjectService::listProjects(const ProjectListFilter& filter)
    {
        std::vector<models::Project> projects = projectDao_->list(filter.page, filter.pageSize);

        if (filter.status || filter.keyword)
        {
            std::vector<models::Project> filtered;
            for (const auto& project : projects)
            {
                bool match = true;

                if (filter.status && project.status != *filter.status)
                {
                    match = false;
                }

                if (filter.keyword)
                {
                    std::string keyword = *filter.keyword;
                    std::string codeLower = project.code;
                    std::string nameLower = project.name;
                    std::transform(codeLower.begin(), codeLower.end(), codeLower.begin(), ::tolower);
                    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                    std::transform(keyword.begin(), keyword.end(), keyword.begin(), ::tolower);

                    if (codeLower.find(keyword) == std::string::npos && nameLower.find(keyword) == std::string::npos)
                    {
                        match = false;
                    }
                }

                if (match)
                {
                    filtered.push_back(project);
                }
            }
            return filtered;
        }

        return projects;
    }

    bool ProjectService::updateProject(int id, const ProjectUpdateParams& params)
    {
        auto projectOpt = projectDao_->findById(id);
        if (!projectOpt)
        {
            throw common::NotFoundException("项目不存在: " + std::to_string(id));
        }

        auto project = *projectOpt;

        if (params.name)
        {
            project.name = *params.name;
        }

        if (params.boundary)
        {
            if (!validateGeometry(*params.boundary))
            {
                throw common::ValidationException("项目边界几何无效");
            }

            auto validGeom = ensureMultiPolygon(*params.boundary);
            validGeom = ensure3D(validGeom);
            project.responsibility_area = validGeom;

            if (!params.center_lon && !params.center_lat)
            {
                auto centroid = calculateCentroid(validGeom);
                if (centroid)
                {
                    project.center_lon = centroid->first;
                    project.center_lat = centroid->second;
                }
            }
        }

        if (params.center_lon)
        {
            project.center_lon = *params.center_lon;
        }

        if (params.center_lat)
        {
            project.center_lat = *params.center_lat;
        }

        if (params.status)
        {
            project.status = *params.status;
        }

        project.updated_at = std::chrono::system_clock::now();

        bool success = projectDao_->update(project);

        if (success)
        {
            utils::Logger::info("Project updated: {}", project.code);
        }

        return success;
    }

    bool ProjectService::deleteProject(int id)
    {
        auto projectOpt = projectDao_->findById(id);
        if (!projectOpt)
        {
            throw common::NotFoundException("项目不存在: " + std::to_string(id));
        }

        auto periods = periodDao_->findByProjectId(id);
        if (!periods.empty())
        {
            throw common::ValidationException("项目存在 " + std::to_string(periods.size()) + " 个监测期，无法删除");
        }

        bool success = projectDao_->remove(id);

        if (success)
        {
            utils::Logger::info("Project deleted: {}", projectOpt->code);
        }

        return success;
    }

    bool ProjectService::uploadBoundary(int projectId, const std::string& filePath, const std::string& format,
                                        std::optional<int> sourceEpsg)
    {
        auto projectOpt = projectDao_->findById(projectId);
        if (!projectOpt)
        {
            throw common::NotFoundException("项目不存在: " + std::to_string(projectId));
        }

        models::Geometry geometry;
        infrastructure::gdal::Geometry gdalGeom;

        if (format == "geojson")
        {
            gdalGeom = gdal_->readGeometryFromGeoJson(filePath);
        }
        else if (format == "shapefile" || format == "shp")
        {
            gdalGeom = gdal_->readGeometryFromShapefile(filePath);
        }
        else if (format == "dxf")
        {
            gdalGeom = gdal_->readGeometryFromDxf(filePath);
        }
        else if (format == "kml" || format == "kmz")
        {
            gdalGeom = gdal_->readGeometryFromKml(filePath);
        }
        else
        {
            geometry.wkt = gdalGeom.wkt;
            throw common::ValidationException("不支持的文件格式: " + format);
        }

        if (!validateGeometry(geometry))
        {
            throw common::ValidationException("上传的边界几何无效");
        }

        geometry = ensureMultiPolygon(geometry);
        geometry = ensure3D(geometry);

        if (sourceEpsg && *sourceEpsg != 4326)
        {
            geometry = transformToWgs84(geometry, *sourceEpsg);
        }

        ProjectUpdateParams params;
        params.boundary = geometry;

        return updateProject(projectId, params);
    }

    std::optional<models::Geometry> ProjectService::getBoundary(int projectId)
    {
        auto projectOpt = projectDao_->findById(projectId);
        if (!projectOpt)
        {
            return std::nullopt;
        }

        const auto& project = *projectOpt;
        if (project.responsibility_area.wkt.empty())
        {
            return std::nullopt;
        }

        return project.responsibility_area;
    }

    double ProjectService::uploadBoundaryContent(int projectId, const std::string& fileName,
                                                 const std::string& content, std::optional<int> sourceEpsg)
    {
        auto projectOpt = projectDao_->findById(projectId);
        if (!projectOpt)
        {
            throw common::NotFoundException("项目不存在: " + std::to_string(projectId));
        }

        std::filesystem::path p(fileName);
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

        if (ext != ".geojson" && ext != ".json" && ext != ".shp" && ext != ".zip" && ext != ".dxf" && ext != ".kml" &&
            ext != ".kmz")
        {
            throw common::ValidationException("不支持的文件格式: " + ext);
        }

        // 落盘临时文件后按路径读取
        std::filesystem::create_directories("uploads/boundaries");
        std::string tmpName = "uploads/boundaries/" + std::to_string(projectId) + "_" +
                              std::to_string(
                                  std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::system_clock::now().time_since_epoch())
                                      .count()) +
                              "_" + p.filename().string();
        {
            std::ofstream out(tmpName, std::ios::binary);
            out.write(content.data(), static_cast<std::streamsize>(content.size()));
        }

        try
        {
            double areaHa = uploadBoundaryFromPath(projectId, tmpName, sourceEpsg);
            std::error_code ec;
            std::filesystem::remove(tmpName, ec);
            return areaHa;
        }
        catch (...)
        {
            std::error_code ec;
            std::filesystem::remove(tmpName, ec);
            throw;
        }
    }

    double ProjectService::uploadBoundaryFromPath(int projectId, const std::string& filePath,
                                                  std::optional<int> sourceEpsg)
    {
        auto projectOpt = projectDao_->findById(projectId);
        if (!projectOpt)
        {
            throw common::NotFoundException("项目不存在: " + std::to_string(projectId));
        }

        std::filesystem::path p(filePath);
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

        // zip 经 /vsizip/ 虚拟路径读取
        std::string readPath = filePath;
        if (ext == ".zip")
            readPath = "/vsizip/" + std::filesystem::absolute(filePath).string();

        models::Geometry geometry;
        infrastructure::gdal::Geometry gdalGeom;

        if (ext == ".geojson" || ext == ".json")
        {
            // 先直接解析 GeoJSON 文本 (Feature/FeatureCollection/Geometry 均可), 失败再走 GDAL 驱动
            std::ifstream in(filePath, std::ios::binary);
            std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            try
            {
                auto j = nlohmann::json::parse(text);
                const nlohmann::json* geomJson = nullptr;
                if (j.contains("type") && j.contains("coordinates"))
                    geomJson = &j;
                else if (j.contains("features") && j["features"].is_array() && !j["features"].empty() &&
                         j["features"][0].contains("geometry"))
                    geomJson = &j["features"][0]["geometry"];
                else if (j.contains("geometry"))
                    geomJson = &j["geometry"];

                if (geomJson)
                {
                    std::string wkt = utils::GeoJsonUtils::geoJsonToWkt(*geomJson);
                    if (!wkt.empty())
                        gdalGeom = infrastructure::gdal::Geometry(wkt, 4326);
                }
            }
            catch (const std::exception&)
            {
            }
            if (gdalGeom.wkt.empty())
                gdalGeom = gdal_->readGeometryFromGeoJson(readPath);
        }
        else if (ext == ".shp" || ext == ".zip")
        {
            gdalGeom = gdal_->readGeometryFromShapefile(readPath);
        }
        else if (ext == ".dxf")
        {
            gdalGeom = gdal_->readGeometryFromDxf(readPath);
        }
        else if (ext == ".kml" || ext == ".kmz")
        {
            gdalGeom = gdal_->readGeometryFromKml(readPath);
        }
        else
        {
            throw common::ValidationException("不支持的文件格式: " + ext);
        }

        if (gdalGeom.wkt.empty())
        {
            throw common::ValidationException("文件中未找到有效多边形几何");
        }

        geometry.wkt = gdalGeom.wkt;
        geometry.srid = "EPSG:" + std::to_string(gdalGeom.srid);

        if (sourceEpsg && *sourceEpsg != 4326)
        {
            geometry = transformToWgs84(geometry, *sourceEpsg);
        }

        if (!validateGeometry(geometry))
        {
            throw common::ValidationException("上传的边界几何无效");
        }

        geometry = ensureMultiPolygon(geometry);
        geometry = ensure3D(geometry);

        ProjectUpdateParams params;
        params.boundary = geometry;
        updateProject(projectId, params);

        double areaHa = utils::GeoJsonUtils::areaHa(geometry.wkt);
        utils::Logger::info("Project boundary uploaded: {} (area_ha={})", projectOpt->code, areaHa);
        return areaHa;
    }

    std::vector<models::ProjectDocument> ProjectService::listDocuments(int projectId)
    {
        auto projectOpt = projectDao_->findById(projectId);
        if (!projectOpt)
        {
            throw common::NotFoundException("项目不存在: " + std::to_string(projectId));
        }
        return documentDao()->findByProject(projectId);
    }

    models::ProjectDocument ProjectService::uploadDocument(int projectId, const std::string& docType,
                                                           const std::optional<std::string>& title,
                                                           const std::string& fileName, const std::string& content)
    {
        auto projectOpt = projectDao_->findById(projectId);
        if (!projectOpt)
        {
            throw common::NotFoundException("项目不存在: " + std::to_string(projectId));
        }
        if (fileName.empty())
        {
            throw common::ValidationException("文件名为空");
        }

        std::filesystem::path base(infrastructure::ConfigManager::getInstance().getStorageRawDir());
        std::filesystem::path saveDir = base / ("project_" + std::to_string(projectId)) / "docs";
        std::filesystem::create_directories(saveDir);
        std::filesystem::path savePath = saveDir / std::filesystem::path(fileName).filename();

        {
            std::ofstream out(savePath, std::ios::binary);
            out.write(content.data(), static_cast<std::streamsize>(content.size()));
        }

        models::ProjectDocument doc;
        doc.project_id = projectId;
        doc.doc_type = docType.empty() ? "other" : docType;
        doc.title = title;
        doc.name = std::filesystem::path(fileName).filename().string();
        doc.file_path = savePath.string();
        doc.file_size_kb = static_cast<double>(content.size()) / 1024.0;
        doc.uploaded_at = std::chrono::system_clock::now();

        int docId = documentDao()->insert(doc);
        if (docId < 0)
        {
            throw common::DatabaseException("项目资料写入失败");
        }
        doc.id = docId;

        utils::Logger::info("Project document uploaded: {} (project {})", doc.name, projectId);
        return doc;
    }

    std::optional<models::ProjectDocument> ProjectService::getDocument(int projectId, int docId)
    {
        auto docOpt = documentDao()->findById(docId);
        if (!docOpt || docOpt->project_id != projectId)
        {
            return std::nullopt;
        }
        return docOpt;
    }

    bool ProjectService::deleteDocument(int projectId, int docId)
    {
        auto docOpt = getDocument(projectId, docId);
        if (!docOpt)
        {
            throw common::NotFoundException("资料不存在: " + std::to_string(docId));
        }

        if (!docOpt->file_path.empty())
        {
            std::error_code ec;
            std::filesystem::remove(docOpt->file_path, ec);
        }

        return documentDao()->remove(docId);
    }

    std::vector<models::MonitoringPeriod> ProjectService::getMonitoringPeriods(int projectId)
    {
        return periodDao_->findByProjectId(projectId);
    }

    int ProjectService::getMonitoringPeriodCount(int projectId)
    {
        auto periods = periodDao_->findByProjectId(projectId);
        return static_cast<int>(periods.size());
    }

    bool ProjectService::validateGeometry(const models::Geometry& geometry)
    {
        if (geometry.wkt.empty())
        {
            return false;
        }

        return gdal_->validateGeometry(geometry.wkt);
    }

    std::optional<std::pair<double, double>> ProjectService::calculateCentroid(const models::Geometry& geometry)
    {
        return gdal_->calculateCentroid(geometry.wkt);
    }

    double ProjectService::calculateArea(const models::Geometry& geometry)
    {
        return gdal_->calculateArea(geometry.wkt);
    }

    models::Geometry ProjectService::transformToWgs84(const models::Geometry& geometry, int sourceEpsg)
    {
        std::string wktWgs84 = gdal_->transformGeometry(geometry.wkt, sourceEpsg, 4326);

        models::Geometry result;
        result.wkt = wktWgs84;
        result.srid = "EPSG:4326";
        result.is_valid = validateGeometry(result);

        return result;
    }

    models::Geometry ProjectService::ensureMultiPolygon(const models::Geometry& geometry)
    {
        std::string mpWkt = gdal_->ensureMultiPolygon(geometry.wkt);

        models::Geometry result;
        result.wkt = mpWkt;
        result.srid = geometry.srid;
        result.is_valid = validateGeometry(result);

        return result;
    }

    models::Geometry ProjectService::ensure3D(const models::Geometry& geometry)
    {
        std::string wkt3D = gdal_->ensure3D(geometry.wkt);

        models::Geometry result;
        result.wkt = wkt3D;
        result.srid = geometry.srid;
        result.is_valid = validateGeometry(result);

        return result;
    }

}  // namespace soilwatercm::services
