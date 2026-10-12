#include "gdal_wrapper.h"

#include <stdexcept>

#include <gdalwarper.h>
#include <ogrsf_frmts.h>

namespace soilwatercm::infrastructure::gdal
{
    void GdalWrapper::init()
    {
        GDALAllRegister();
    }

    void GdalWrapper::cleanup()
    {
        GDALDestroyDriverManager();
    }

    GdalWrapper::GdalWrapper()
    {
        init();
    }

    GdalWrapper::~GdalWrapper() {}

    RasterData GdalWrapper::readRaster(const std::string& file_path)
    {
        RasterData result;

        GDALDataset* dataset = static_cast<GDALDataset*>(GDALOpen(file_path.c_str(), GA_ReadOnly));

        if (!dataset)
        {
            throw std::runtime_error("Failed to open raster file: " + file_path + " - " + getLastError());
        }

        std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset_guard(dataset, GDALClose);

        result.width = dataset->GetRasterXSize();
        result.height = dataset->GetRasterYSize();
        result.bands = dataset->GetRasterCount();

        dataset->GetGeoTransform(result.geotransform.data());

        const char* projection = dataset->GetProjectionRef();
        if (projection)
        {
            result.projection = projection;
        }

        if (result.bands > 0)
        {
            GDALRasterBand* band = dataset->GetRasterBand(1);
            if (band)
            {
                result.data_type = band->GetRasterDataType();
            }
        }

        result.data.resize(static_cast<size_t>(result.width) * result.height * result.bands);

        for (int band_idx = 1; band_idx <= result.bands; ++band_idx)
        {
            GDALRasterBand* band = dataset->GetRasterBand(band_idx);
            if (!band)
            {
                continue;
            }

            size_t offset = static_cast<size_t>(band_idx - 1) * result.width * result.height;

            CPLErr err = band->RasterIO(GF_Read, 0, 0, result.width, result.height, result.data.data() + offset,
                                        result.width, result.height, GDT_Float64, 0, 0);

            if (err != CE_None)
            {
                throw std::runtime_error("Failed to read raster band " + std::to_string(band_idx));
            }
        }

        return result;
    }

    RasterData GdalWrapper::readRaster(const std::string& file_path, int bandIndex)
    {
        RasterData result;

        GDALDataset* dataset = static_cast<GDALDataset*>(GDALOpen(file_path.c_str(), GA_ReadOnly));

        if (!dataset)
        {
            throw std::runtime_error("Failed to open raster file: " + file_path + " - " + getLastError());
        }

        std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset_guard(dataset, GDALClose);

        result.width = dataset->GetRasterXSize();
        result.height = dataset->GetRasterYSize();
        result.bands = 1;

        dataset->GetGeoTransform(result.geotransform.data());

        const char* projection = dataset->GetProjectionRef();
        if (projection)
        {
            result.projection = projection;
        }

        if (bandIndex < 1 || bandIndex > dataset->GetRasterCount())
        {
            throw std::runtime_error("Invalid band index: " + std::to_string(bandIndex));
        }

        GDALRasterBand* band = dataset->GetRasterBand(bandIndex);
        if (band)
        {
            result.data_type = band->GetRasterDataType();
        }

        result.data.resize(static_cast<size_t>(result.width) * result.height);

        if (band)
        {
            CPLErr err = band->RasterIO(GF_Read, 0, 0, result.width, result.height, result.data.data(), result.width,
                                        result.height, GDT_Float64, 0, 0);

            if (err != CE_None)
            {
                throw std::runtime_error("Failed to read raster band " + std::to_string(bandIndex));
            }
        }

        return result;
    }

    bool GdalWrapper::writeRaster(const std::string& file_path, const RasterData& data, const std::string& format)
    {
        GDALDriver* driver = GetGDALDriverManager()->GetDriverByName(format.c_str());
        if (!driver)
        {
            throw std::runtime_error("Failed to get driver: " + format);
        }

        GDALDataset* dataset =
            driver->Create(file_path.c_str(), data.width, data.height, data.bands, data.data_type, nullptr);

        if (!dataset)
        {
            throw std::runtime_error("Failed to create raster file: " + file_path);
        }

        std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset_guard(dataset, GDALClose);

        dataset->SetGeoTransform(data.geotransform.data());

        if (!data.projection.empty())
        {
            dataset->SetProjection(data.projection.c_str());
        }

        for (int band_idx = 1; band_idx <= data.bands; ++band_idx)
        {
            GDALRasterBand* band = dataset->GetRasterBand(band_idx);
            if (!band)
            {
                continue;
            }

            size_t offset = static_cast<size_t>(band_idx - 1) * data.width * data.height;

            CPLErr err =
                band->RasterIO(GF_Write, 0, 0, data.width, data.height, const_cast<double*>(data.data.data() + offset),
                               data.width, data.height, GDT_Float64, 0, 0);

            if (err != CE_None)
            {
                return false;
            }
        }

        return true;
    }

    std::string GdalWrapper::getProjection(const std::string& file_path)
    {
        GDALDataset* dataset = static_cast<GDALDataset*>(GDALOpen(file_path.c_str(), GA_ReadOnly));

        if (!dataset)
        {
            return "";
        }

        std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset_guard(dataset, GDALClose);

        const char* projection = dataset->GetProjectionRef();
        return projection ? std::string(projection) : "";
    }

    bool GdalWrapper::setProjection(const std::string& file_path, const std::string& projection)
    {
        GDALDataset* dataset = static_cast<GDALDataset*>(GDALOpen(file_path.c_str(), GA_Update));

        if (!dataset)
        {
            return false;
        }

        std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset_guard(dataset, GDALClose);

        return dataset->SetProjection(projection.c_str()) == CE_None;
    }

    std::vector<double> GdalWrapper::getGeoTransform(const std::string& file_path)
    {
        std::vector<double> transform(6);

        GDALDataset* dataset = static_cast<GDALDataset*>(GDALOpen(file_path.c_str(), GA_ReadOnly));

        if (!dataset)
        {
            return transform;
        }

        std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset_guard(dataset, GDALClose);

        dataset->GetGeoTransform(transform.data());
        return transform;
    }

    bool GdalWrapper::setGeoTransform(const std::string& file_path, const std::vector<double>& transform)
    {
        if (transform.size() != 6)
        {
            return false;
        }

        GDALDataset* dataset = static_cast<GDALDataset*>(GDALOpen(file_path.c_str(), GA_Update));

        if (!dataset)
        {
            return false;
        }

        std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset_guard(dataset, GDALClose);

        return dataset->SetGeoTransform(const_cast<double*>(transform.data())) == CE_None;
    }

    std::string GdalWrapper::reproject(const std::string& input_file, const std::string& output_file,
                                       const std::string& target_srs)
    {
        GDALDatasetH srcDataset = GDALOpen(input_file.c_str(), GA_ReadOnly);
        if (!srcDataset)
        {
            throw std::runtime_error("Failed to open input file for reprojection: " + input_file + " - " +
                                     getLastError());
        }

        GDALDataset* srcDS = static_cast<GDALDataset*>(srcDataset);

        OGRSpatialReference targetSrsObj;
        if (target_srs.find("EPSG:") == 0)
        {
            int epsg = std::stoi(target_srs.substr(5));
            targetSrsObj.importFromEPSG(epsg);
        }
        else
        {
            targetSrsObj.importFromWkt(target_srs.c_str());
        }
        targetSrsObj.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        char* targetWkt = nullptr;
        targetSrsObj.exportToWkt(&targetWkt);

        const char* srcWkt = srcDS->GetProjectionRef();

        double gt[6];
        srcDS->GetGeoTransform(gt);

        int xSize = srcDS->GetRasterXSize();
        int ySize = srcDS->GetRasterYSize();

        double minX = gt[0];
        double maxY = gt[3];
        double maxX = gt[0] + gt[1] * xSize;
        double minY = gt[3] + gt[5] * ySize;

        OGRSpatialReference srcSrsObj;
        if (srcWkt && strlen(srcWkt) > 0)
        {
            srcSrsObj.importFromWkt(srcWkt);
        }
        else
        {
            srcSrsObj.importFromEPSG(4326);
        }
        srcSrsObj.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        OGRCoordinateTransformation* coordTrans = OGRCreateCoordinateTransformation(&srcSrsObj, &targetSrsObj);
        if (!coordTrans)
        {
            CPLFree(targetWkt);
            GDALClose(srcDataset);
            throw std::runtime_error("Failed to create coordinate transformation");
        }

        double corners[8] = {minX, maxY, maxX, maxY, maxX, minY, minX, minY};
        coordTrans->Transform(4, corners, corners + 1);
        delete coordTrans;

        double newMinX = std::min({corners[0], corners[2], corners[4], corners[6]});
        double newMaxX = std::max({corners[0], corners[2], corners[4], corners[6]});
        double newMinY = std::min({corners[1], corners[3], corners[5], corners[7]});
        double newMaxY = std::max({corners[1], corners[3], corners[5], corners[7]});

        double resolution = std::abs(gt[1]);
        int newXSize = static_cast<int>(std::ceil((newMaxX - newMinX) / resolution));
        int newYSize = static_cast<int>(std::ceil((newMaxY - newMinY) / resolution));

        if (newXSize <= 0)
            newXSize = xSize;
        if (newYSize <= 0)
            newYSize = ySize;

        GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
        if (!driver)
        {
            CPLFree(targetWkt);
            GDALClose(srcDataset);
            throw std::runtime_error("GTiff driver not available");
        }

        char** papszOptions = nullptr;
        papszOptions = CSLSetNameValue(papszOptions, "COMPRESS", "LZW");
        papszOptions = CSLSetNameValue(papszOptions, "TILED", "YES");

        GDALDataset* dstDataset = driver->Create(output_file.c_str(), newXSize, newYSize, srcDS->GetRasterCount(),
                                                 srcDS->GetRasterBand(1)->GetRasterDataType(), papszOptions);
        CSLDestroy(papszOptions);

        if (!dstDataset)
        {
            CPLFree(targetWkt);
            GDALClose(srcDataset);
            throw std::runtime_error("Failed to create output file: " + output_file);
        }

        double dstGt[6] = {newMinX, resolution, 0.0, newMaxY, 0.0, -resolution};
        dstDataset->SetGeoTransform(dstGt);
        dstDataset->SetProjection(targetWkt);
        std::string targetWktStr = targetWkt;
        CPLFree(targetWkt);

        CPLErr err = GDALReprojectImage(srcDataset, srcWkt, dstDataset, targetWktStr.c_str(), GRA_Bilinear, 0.0, 0.0,
                                        nullptr, nullptr, nullptr);

        GDALClose(dstDataset);
        GDALClose(srcDataset);

        if (err != CE_None)
        {
            throw std::runtime_error("Reprojection failed: " + std::string(CPLGetLastErrorMsg()));
        }

        return output_file;
    }

    std::string GdalWrapper::getDriverName(const std::string& file_path)
    {
        GDALDataset* dataset = static_cast<GDALDataset*>(GDALOpen(file_path.c_str(), GA_ReadOnly));

        if (!dataset)
        {
            return "";
        }

        std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset_guard(dataset, GDALClose);

        GDALDriver* driver = dataset->GetDriver();
        return driver ? std::string(driver->GetDescription()) : "";
    }

    std::vector<std::string> GdalWrapper::getMetadata(const std::string& file_path)
    {
        std::vector<std::string> metadata;

        GDALDataset* dataset = static_cast<GDALDataset*>(GDALOpen(file_path.c_str(), GA_ReadOnly));

        if (!dataset)
        {
            return metadata;
        }

        std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset_guard(dataset, GDALClose);

        CSLConstList metadata_items = dataset->GetMetadata();
        if (metadata_items)
        {
            for (int i = 0; metadata_items[i] != nullptr; ++i)
            {
                metadata.emplace_back(metadata_items[i]);
            }
        }

        return metadata;
    }

    std::string GdalWrapper::reproject(const std::string& input_file, const std::string& output_file, int target_epsg)
    {
        OGRSpatialReference targetSrs;
        targetSrs.importFromEPSG(target_epsg);
        char* targetWkt = nullptr;
        targetSrs.exportToWkt(&targetWkt);
        std::string targetSrsStr(targetWkt);
        CPLFree(targetWkt);
        return reproject(input_file, output_file, targetSrsStr);
    }

    RasterInfo GdalWrapper::getRasterInfo(const std::string& file_path)
    {
        RasterInfo info;

        GDALDataset* dataset = static_cast<GDALDataset*>(GDALOpen(file_path.c_str(), GA_ReadOnly));

        if (!dataset)
        {
            return info;
        }

        std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset_guard(dataset, GDALClose);

        info.width = dataset->GetRasterXSize();
        info.height = dataset->GetRasterYSize();
        info.bandCount = dataset->GetRasterCount();

        double gt[6];
        if (dataset->GetGeoTransform(gt) == CE_None)
        {
            info.xMin = gt[0];
            info.yMin = gt[3] + gt[5] * info.height;
            info.xMax = gt[0] + gt[1] * info.width;
            info.yMax = gt[3];
            info.resolution = std::abs(gt[1]);
        }

        const char* proj = dataset->GetProjectionRef();
        if (proj)
        {
            info.crs = proj;
        }

        return info;
    }

    OGRGeometry* GdalWrapper::parseWkt(const std::string& wkt)
    {
        if (wkt.empty())
        {
            return nullptr;
        }
        OGRGeometry* geom = nullptr;
        OGRGeometryFactory::createFromWkt(wkt.c_str(), nullptr, &geom);
        return geom;
    }

    std::string GdalWrapper::geometryToWkt(OGRGeometry* geom)
    {
        if (!geom)
        {
            return "";
        }
        char* wkt = nullptr;
        geom->exportToWkt(&wkt);
        std::string result(wkt ? wkt : "");
        if (wkt)
        {
            CPLFree(wkt);
        }
        return result;
    }

    bool GdalWrapper::validateGeometry(const std::string& wkt)
    {
        std::unique_ptr<OGRGeometry, decltype(&OGRGeometryFactory::destroyGeometry)> geom(
            parseWkt(wkt), OGRGeometryFactory::destroyGeometry);
        if (!geom)
        {
            return false;
        }
        return geom->IsValid();
    }

    std::optional<std::pair<double, double>> GdalWrapper::calculateCentroid(const std::string& wkt)
    {
        std::unique_ptr<OGRGeometry, decltype(&OGRGeometryFactory::destroyGeometry)> geom(
            parseWkt(wkt), OGRGeometryFactory::destroyGeometry);
        if (!geom)
        {
            return std::nullopt;
        }

        OGRPoint centroid;
        if (geom->Centroid(&centroid) != OGRERR_NONE)
        {
            return std::nullopt;
        }

        return std::make_pair(centroid.getX(), centroid.getY());
    }

    double GdalWrapper::calculateArea(const std::string& wkt)
    {
        std::unique_ptr<OGRGeometry, decltype(&OGRGeometryFactory::destroyGeometry)> geom(
            parseWkt(wkt), OGRGeometryFactory::destroyGeometry);
        if (!geom)
        {
            return 0.0;
        }

        OGRwkbGeometryType type = geom->getGeometryType();
        if (wkbFlatten(type) == wkbPolygon)
        {
            return geom->toPolygon()->get_Area();
        }
        else if (wkbFlatten(type) == wkbMultiPolygon)
        {
            OGRMultiPolygon* mp = geom->toMultiPolygon();
            double area = 0.0;
            for (int i = 0; i < mp->getNumGeometries(); ++i)
            {
                OGRPolygon* poly = mp->getGeometryRef(i)->toPolygon();
                if (poly)
                {
                    area += poly->get_Area();
                }
            }
            return area;
        }

        if (auto* cp = geom->toCurvePolygon())
            return cp->get_Area();
        return 0.0;
    }

    double GdalWrapper::calculateLength(const std::string& wkt)
    {
        std::unique_ptr<OGRGeometry, decltype(&OGRGeometryFactory::destroyGeometry)> geom(
            parseWkt(wkt), OGRGeometryFactory::destroyGeometry);
        if (!geom)
        {
            return 0.0;
        }
        if (auto* c = geom->toCurve())
            return c->get_Length();
        if (auto* mc = geom->toMultiCurve())
            return mc->get_Length();
        if (auto* mls = geom->toMultiLineString())
            return mls->get_Length();
        return 0.0;
    }

    std::string GdalWrapper::transformGeometry(const std::string& wkt, int sourceEpsg, int targetEpsg)
    {
        std::unique_ptr<OGRGeometry, decltype(&OGRGeometryFactory::destroyGeometry)> geom(
            parseWkt(wkt), OGRGeometryFactory::destroyGeometry);
        if (!geom)
        {
            return "";
        }

        OGRSpatialReference sourceSrs;
        sourceSrs.importFromEPSG(sourceEpsg);
        sourceSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        OGRSpatialReference targetSrs;
        targetSrs.importFromEPSG(targetEpsg);
        targetSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        OGRCoordinateTransformation* transform = OGRCreateCoordinateTransformation(&sourceSrs, &targetSrs);
        if (!transform)
        {
            return "";
        }

        geom->transform(transform);
        delete transform;

        return geometryToWkt(geom.get());
    }

    std::string GdalWrapper::ensureMultiPolygon(const std::string& wkt)
    {
        std::unique_ptr<OGRGeometry, decltype(&OGRGeometryFactory::destroyGeometry)> geom(
            parseWkt(wkt), OGRGeometryFactory::destroyGeometry);
        if (!geom)
        {
            return wkt;
        }

        OGRwkbGeometryType type = geom->getGeometryType();
        if (wkbFlatten(type) == wkbMultiPolygon)
        {
            return wkt;
        }

        if (wkbFlatten(type) == wkbPolygon)
        {
            OGRMultiPolygon* mp = new OGRMultiPolygon();
            mp->addGeometry(geom.get());
            std::string result = geometryToWkt(mp);
            delete mp;
            return result;
        }

        if (wkbFlatten(type) == wkbGeometryCollection)
        {
            OGRMultiPolygon* mp = new OGRMultiPolygon();
            OGRGeometryCollection* gc = static_cast<OGRGeometryCollection*>(geom.get());
            for (int i = 0; i < gc->getNumGeometries(); ++i)
            {
                OGRGeometry* sub = gc->getGeometryRef(i);
                OGRwkbGeometryType subType = sub->getGeometryType();
                if (wkbFlatten(subType) == wkbPolygon)
                {
                    mp->addGeometry(sub);
                }
                else if (wkbFlatten(subType) == wkbMultiPolygon)
                {
                    OGRMultiPolygon* subMp = static_cast<OGRMultiPolygon*>(sub);
                    for (int j = 0; j < subMp->getNumGeometries(); ++j)
                    {
                        mp->addGeometry(subMp->getGeometryRef(j));
                    }
                }
            }
            if (mp->getNumGeometries() > 0)
            {
                std::string result = geometryToWkt(mp);
                delete mp;
                return result;
            }
            delete mp;
        }

        return wkt;
    }

    std::string GdalWrapper::ensure3D(const std::string& wkt)
    {
        std::unique_ptr<OGRGeometry, decltype(&OGRGeometryFactory::destroyGeometry)> geom(
            parseWkt(wkt), OGRGeometryFactory::destroyGeometry);
        if (!geom)
        {
            return wkt;
        }

        OGRwkbGeometryType type = geom->getGeometryType();
        if (wkbHasZ(type))
        {
            return wkt;
        }

        geom->setCoordinateDimension(3);
        return geometryToWkt(geom.get());
    }

    Geometry GdalWrapper::readGeometryFromGeoJson(const std::string& filePath)
    {
        GDALDataset* dataset =
            static_cast<GDALDataset*>(GDALOpenEx(filePath.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr));
        if (!dataset)
        {
            return {};
        }

        std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset_guard(dataset, GDALClose);

        OGRLayer* layer = dataset->GetLayer(0);
        if (!layer)
        {
            return {};
        }

        OGRFeature* feature = layer->GetNextFeature();
        if (!feature)
        {
            return {};
        }

        OGRGeometry* geom = feature->GetGeometryRef();
        std::string wkt = geometryToWkt(geom);

        int srid = 4326;
        const OGRSpatialReference* srs = layer->GetSpatialRef();
        if (srs)
        {
            const char* authCode = srs->GetAuthorityCode(nullptr);
            if (authCode)
            {
                srid = std::stoi(authCode);
            }
        }

        OGRFeature::DestroyFeature(feature);
        return Geometry(wkt, srid);
    }

    Geometry GdalWrapper::readGeometryFromShapefile(const std::string& filePath)
    {
        return readGeometryFromGeoJson(filePath);
    }

    Geometry GdalWrapper::readGeometryFromDxf(const std::string& filePath)
    {
        return readGeometryFromGeoJson(filePath);
    }

    Geometry GdalWrapper::readGeometryFromKml(const std::string& filePath)
    {
        return readGeometryFromGeoJson(filePath);
    }

    std::string GdalWrapper::getLastError()
    {
        return std::string(CPLGetLastErrorMsg());
    }

}  // namespace soilwatercm::infrastructure::gdal
