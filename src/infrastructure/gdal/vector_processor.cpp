#include "vector_processor.h"

#include <cmath>
#include <stdexcept>

#include <gdal_alg.h>
#include <ogr_geometry.h>
#include <ogr_spatialref.h>

namespace soilwatercm::infrastructure::gdal
{

    VectorProcessor::VectorProcessor() {}

    VectorData VectorProcessor::read(const std::string& file_path)
    {
        VectorData result;

        GDALDataset* dataset =
            static_cast<GDALDataset*>(GDALOpenEx(file_path.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr));

        if (!dataset)
        {
            return result;
        }

        std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset_guard(dataset, GDALClose);

        OGRLayer* layer = dataset->GetLayer(0);
        if (!layer)
        {
            return result;
        }

        if (layer->GetSpatialRef())
        {
            result.projection = layer->GetSpatialRef()->exportToWkt();
        }

        OGRFeatureDefn* feature_defn = layer->GetLayerDefn();
        result.geometry_type = feature_defn->GetGeomType() == wkbPoint        ? "Point"
                               : feature_defn->GetGeomType() == wkbLineString ? "LineString"
                               : feature_defn->GetGeomType() == wkbPolygon    ? "Polygon"
                                                                              : "Unknown";

        layer->ResetReading();
        OGRFeature* feature;
        while ((feature = layer->GetNextFeature()) != nullptr)
        {
            VectorFeature vf;

            OGRGeometry* geometry = feature->GetGeometryRef();
            if (geometry)
            {
                char* wkt = nullptr;
                geometry->exportToWkt(&wkt);
                if (wkt)
                {
                    vf.geometry = wkt;
                    CPLFree(wkt);
                }
            }

            for (int i = 0; i < feature_defn->GetFieldCount(); ++i)
            {
                OGRFieldDefn* field_defn = feature_defn->GetFieldDefn(i);
                const char* field_name = field_defn->GetNameRef();
                const char* field_value = feature->GetFieldAsString(i);

                if (field_name && field_value)
                {
                    vf.attributes[field_name] = field_value;
                }
            }

            result.features.push_back(vf);
            OGRFeature::DestroyFeature(feature);
        }

        return result;
    }

    bool VectorProcessor::write(const std::string& file_path, const VectorData& data, const std::string& format)
    {
        GDALDriver* driver = GetGDALDriverManager()->GetDriverByName(format.c_str());
        if (!driver)
        {
            return false;
        }

        GDALDataset* dataset = driver->Create(file_path.c_str(), 0, 0, 0, GDT_Unknown, nullptr);
        if (!dataset)
        {
            return false;
        }

        std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset_guard(dataset, GDALClose);

        OGRLayer* layer = dataset->CreateLayer("output", nullptr, wkbPolygon, nullptr);
        if (!layer)
        {
            return false;
        }

        OGRFieldDefn idField("id", OFTInteger);
        layer->CreateField(&idField);

        for (size_t i = 0; i < data.features.size(); ++i)
        {
            OGRFeature* feature = OGRFeature::CreateFeature(layer->GetLayerDefn());
            feature->SetField("id", static_cast<int>(i));

            if (!data.features[i].geometry.empty())
            {
                OGRGeometry* geom = nullptr;
                OGRGeometryFactory::createFromWkt(data.features[i].geometry.c_str(), nullptr, &geom);
                feature->SetGeometry(geom);
                OGRGeometryFactory::destroyGeometry(geom);
            }

            layer->CreateFeature(feature);
            OGRFeature::DestroyFeature(feature);
        }

        return true;
    }

    VectorData VectorProcessor::buffer(const VectorData& input, double distance)
    {
        VectorData result;
        result.projection = input.projection;
        result.geometry_type = input.geometry_type;

        for (const auto& feature : input.features)
        {
            VectorFeature bf = feature;
            OGRGeometry* geom = nullptr;
            OGRGeometryFactory::createFromWkt(feature.geometry.c_str(), nullptr, &geom);
            if (geom)
            {
                OGRGeometry* buffered = geom->Buffer(distance);
                if (buffered)
                {
                    char* wkt = nullptr;
                    buffered->exportToWkt(&wkt);
                    if (wkt)
                    {
                        bf.geometry = wkt;
                        CPLFree(wkt);
                    }
                    OGRGeometryFactory::destroyGeometry(buffered);
                }
                OGRGeometryFactory::destroyGeometry(geom);
            }
            result.features.push_back(bf);
        }

        return result;
    }

    VectorData VectorProcessor::intersect(const VectorData& input1, const VectorData& input2)
    {
        VectorData result;
        result.projection = input1.projection;
        result.geometry_type = "Polygon";

        for (const auto& f1 : input1.features)
        {
            OGRGeometry* g1 = nullptr;
            OGRGeometryFactory::createFromWkt(f1.geometry.c_str(), nullptr, &g1);
            if (!g1)
                continue;

            for (const auto& f2 : input2.features)
            {
                OGRGeometry* g2 = nullptr;
                OGRGeometryFactory::createFromWkt(f2.geometry.c_str(), nullptr, &g2);
                if (!g2)
                {
                    OGRGeometryFactory::destroyGeometry(g1);
                    continue;
                }

                OGRGeometry* intersection = g1->Intersection(g2);
                if (intersection && !intersection->IsEmpty())
                {
                    VectorFeature vf;
                    char* wkt = nullptr;
                    intersection->exportToWkt(&wkt);
                    if (wkt)
                    {
                        vf.geometry = wkt;
                        CPLFree(wkt);
                    }
                    vf.attributes = f1.attributes;
                    result.features.push_back(vf);
                }

                OGRGeometryFactory::destroyGeometry(intersection);
                OGRGeometryFactory::destroyGeometry(g2);
            }
            OGRGeometryFactory::destroyGeometry(g1);
        }

        return result;
    }

    VectorData VectorProcessor::union_(const VectorData& input1, const VectorData& input2)
    {
        VectorData result;
        result.projection = input1.projection;
        result.geometry_type = "Polygon";

        OGRMultiPolygon* mp = new OGRMultiPolygon();

        for (const auto& f : input1.features)
        {
            OGRGeometry* g = nullptr;
            OGRGeometryFactory::createFromWkt(f.geometry.c_str(), nullptr, &g);
            if (g)
            {
                mp->addGeometry(g);
                OGRGeometryFactory::destroyGeometry(g);
            }
        }

        for (const auto& f : input2.features)
        {
            OGRGeometry* g = nullptr;
            OGRGeometryFactory::createFromWkt(f.geometry.c_str(), nullptr, &g);
            if (g)
            {
                mp->addGeometry(g);
                OGRGeometryFactory::destroyGeometry(g);
            }
        }

        OGRGeometry* unioned = mp->UnionCascaded();
        if (unioned)
        {
            VectorFeature vf;
            char* wkt = nullptr;
            unioned->exportToWkt(&wkt);
            if (wkt)
            {
                vf.geometry = wkt;
                CPLFree(wkt);
            }
            result.features.push_back(vf);
            OGRGeometryFactory::destroyGeometry(unioned);
        }

        delete mp;
        return result;
    }

    VectorData VectorProcessor::difference(const VectorData& input1, const VectorData& input2)
    {
        VectorData result;
        result.projection = input1.projection;
        result.geometry_type = "Polygon";

        for (const auto& f1 : input1.features)
        {
            OGRGeometry* g1 = nullptr;
            OGRGeometryFactory::createFromWkt(f1.geometry.c_str(), nullptr, &g1);
            if (!g1)
                continue;

            OGRGeometry* diff = g1;
            for (const auto& f2 : input2.features)
            {
                OGRGeometry* g2 = nullptr;
                OGRGeometryFactory::createFromWkt(f2.geometry.c_str(), nullptr, &g2);
                if (g2)
                {
                    OGRGeometry* newDiff = diff->Difference(g2);
                    if (diff != g1)
                        OGRGeometryFactory::destroyGeometry(diff);
                    diff = newDiff;
                    OGRGeometryFactory::destroyGeometry(g2);
                }
            }

            if (diff && !diff->IsEmpty())
            {
                VectorFeature vf;
                char* wkt = nullptr;
                diff->exportToWkt(&wkt);
                if (wkt)
                {
                    vf.geometry = wkt;
                    CPLFree(wkt);
                }
                vf.attributes = f1.attributes;
                result.features.push_back(vf);
            }

            if (diff != g1)
                OGRGeometryFactory::destroyGeometry(diff);
            OGRGeometryFactory::destroyGeometry(g1);
        }

        return result;
    }

    double VectorProcessor::calculateArea(const VectorFeature& feature)
    {
        OGRGeometry* geom = nullptr;
        OGRGeometryFactory::createFromWkt(feature.geometry.c_str(), nullptr, &geom);
        if (!geom)
            return 0.0;

        double area = 0.0;
        const auto areaGt = wkbFlatten(geom->getGeometryType());
        if (OGR_GT_IsSubClassOf(areaGt, wkbCurvePolygon))
        {
            area = static_cast<OGRCurvePolygon*>(geom)->get_Area();
        }
        else if (OGR_GT_IsSubClassOf(areaGt, wkbGeometryCollection))
        {
            area = static_cast<OGRGeometryCollection*>(geom)->get_Area();
        }
        OGRGeometryFactory::destroyGeometry(geom);
        return area;
    }

    double VectorProcessor::calculateLength(const VectorFeature& feature)
    {
        OGRGeometry* geom = nullptr;
        OGRGeometryFactory::createFromWkt(feature.geometry.c_str(), nullptr, &geom);
        if (!geom)
            return 0.0;

        double length = 0.0;
        const auto lenGt = wkbFlatten(geom->getGeometryType());
        if (OGR_GT_IsSubClassOf(lenGt, wkbCurve))
        {
            length = static_cast<OGRCurve*>(geom)->get_Length();
        }
        else if (OGR_GT_IsSubClassOf(lenGt, wkbGeometryCollection))
        {
            length = static_cast<OGRGeometryCollection*>(geom)->get_Length();
        }
        OGRGeometryFactory::destroyGeometry(geom);
        return length;
    }

    double VectorProcessor::calculateDistance(const VectorFeature& feature1, const VectorFeature& feature2)
    {
        OGRGeometry* g1 = nullptr;
        OGRGeometry* g2 = nullptr;
        OGRGeometryFactory::createFromWkt(feature1.geometry.c_str(), nullptr, &g1);
        OGRGeometryFactory::createFromWkt(feature2.geometry.c_str(), nullptr, &g2);

        if (!g1 || !g2)
        {
            if (g1)
                OGRGeometryFactory::destroyGeometry(g1);
            if (g2)
                OGRGeometryFactory::destroyGeometry(g2);
            return 0.0;
        }

        double dist = g1->Distance(g2);
        OGRGeometryFactory::destroyGeometry(g1);
        OGRGeometryFactory::destroyGeometry(g2);
        return dist;
    }

    VectorData VectorProcessor::reproject(const VectorData& input, const std::string& target_srs)
    {
        VectorData result;
        result.projection = target_srs;
        result.geometry_type = input.geometry_type;

        OGRSpatialReference srcSrs;
        if (!input.projection.empty())
        {
            srcSrs.importFromWkt(input.projection.c_str());
        }
        else
        {
            srcSrs.importFromEPSG(4326);
        }
        srcSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        OGRSpatialReference dstSrs;
        if (target_srs.find("EPSG:") == 0)
        {
            int epsg = std::stoi(target_srs.substr(5));
            dstSrs.importFromEPSG(epsg);
        }
        else
        {
            dstSrs.importFromWkt(target_srs.c_str());
        }
        dstSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        OGRCoordinateTransformation* transform = OGRCreateCoordinateTransformation(&srcSrs, &dstSrs);
        if (!transform)
        {
            return input;
        }

        for (const auto& feature : input.features)
        {
            VectorFeature rf = feature;
            OGRGeometry* geom = nullptr;
            OGRGeometryFactory::createFromWkt(feature.geometry.c_str(), nullptr, &geom);
            if (geom)
            {
                geom->transform(transform);
                char* wkt = nullptr;
                geom->exportToWkt(&wkt);
                if (wkt)
                {
                    rf.geometry = wkt;
                    CPLFree(wkt);
                }
                OGRGeometryFactory::destroyGeometry(geom);
            }
            result.features.push_back(rf);
        }

        delete transform;
        return result;
    }

    VectorData VectorProcessor::fromRaster(const RasterData& raster, double threshold)
    {
        VectorData result;
        result.geometry_type = "Polygon";

        if (raster.data.empty() || raster.width == 0 || raster.height == 0)
        {
            return result;
        }

        GDALDriver* memDriver = GetGDALDriverManager()->GetDriverByName("MEM");
        if (!memDriver)
            return result;

        GDALDataset* ds = memDriver->Create("", raster.width, raster.height, 1, GDT_Byte, nullptr);
        if (!ds)
            return result;

        std::unique_ptr<GDALDataset, decltype(&GDALClose)> ds_guard(ds, GDALClose);

        double gt[6] = {raster.geotransform[0], raster.geotransform[1], raster.geotransform[2],
                        raster.geotransform[3], raster.geotransform[4], raster.geotransform[5]};
        ds->SetGeoTransform(gt);
        if (!raster.projection.empty())
        {
            ds->SetProjection(raster.projection.c_str());
        }

        std::vector<GByte> byteData(raster.width * raster.height);
        for (size_t i = 0; i < raster.data.size() && i < byteData.size(); ++i)
        {
            byteData[i] = (raster.data[i] > threshold) ? 1 : 0;
        }

        GDALRasterBand* band = ds->GetRasterBand(1);
        band->RasterIO(GF_Write, 0, 0, raster.width, raster.height, byteData.data(), raster.width, raster.height,
                       GDT_Byte, 0, 0);

        OGRLayer* layer = nullptr;
        GDALDataset* polyDs = GDALDataset::Open(raster.projection.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr);
        (void)polyDs;

        char** papszOptions = nullptr;
        papszOptions = CSLSetNameValue(papszOptions, "8CONNECTED", "8");

        GDALPolygonize(band, band, layer, 0, papszOptions, nullptr, nullptr);

        CSLDestroy(papszOptions);

        return result;
    }

    std::vector<VectorFeature> VectorProcessor::filter(const VectorData& input, const std::string& attribute,
                                                       const std::string& value)
    {
        std::vector<VectorFeature> result;

        for (const auto& feature : input.features)
        {
            auto it = feature.attributes.find(attribute);
            if (it != feature.attributes.end() && it->second == value)
            {
                result.push_back(feature);
            }
        }

        return result;
    }

    std::vector<soilwatercm::models::Geometry> VectorProcessor::rasterToVector(const std::string& rasterPath)
    {
        std::vector<soilwatercm::models::Geometry> result;

        GDALDataset* ds = static_cast<GDALDataset*>(GDALOpen(rasterPath.c_str(), GA_ReadOnly));
        if (!ds)
            return result;

        std::unique_ptr<GDALDataset, decltype(&GDALClose)> ds_guard(ds, GDALClose);

        GDALRasterBand* band = ds->GetRasterBand(1);
        if (!band)
            return result;

        OGRLayer* polyLayer = nullptr;
        GDALDriver* memDriver = GetGDALDriverManager()->GetDriverByName("MEM");
        GDALDataset* memDs = memDriver ? memDriver->Create("", 0, 0, 0, GDT_Unknown, nullptr) : nullptr;
        if (memDs)
        {
            polyLayer = memDs->CreateLayer("polygons", ds->GetSpatialRef(), wkbPolygon, nullptr);
        }

        if (!polyLayer)
        {
            if (memDs)
                GDALClose(memDs);
            return result;
        }

        char** papszOptions = nullptr;
        papszOptions = CSLSetNameValue(papszOptions, "8CONNECTED", "8");

        CPLErr err = GDALPolygonize(band, band, polyLayer, 0, papszOptions, nullptr, nullptr);
        CSLDestroy(papszOptions);

        if (err != CE_None)
        {
            GDALClose(memDs);
            return result;
        }

        polyLayer->ResetReading();
        OGRFeature* feature;
        while ((feature = polyLayer->GetNextFeature()) != nullptr)
        {
            OGRGeometry* geom = feature->GetGeometryRef();
            if (geom && !geom->IsEmpty())
            {
                char* wkt = nullptr;
                geom->exportToWkt(&wkt);
                if (wkt)
                {
                    soilwatercm::models::Geometry g;
                    g.wkt = wkt;
                    g.srid = "EPSG:4326";
                    result.push_back(g);
                    CPLFree(wkt);
                }
            }
            OGRFeature::DestroyFeature(feature);
        }

        GDALClose(memDs);
        return result;
    }

    std::vector<soilwatercm::models::Geometry> VectorProcessor::detectFeatures(const std::string& imagePath)
    {
        std::vector<soilwatercm::models::Geometry> result;

        GDALDataset* ds = static_cast<GDALDataset*>(GDALOpen(imagePath.c_str(), GA_ReadOnly));
        if (!ds)
            return result;

        std::unique_ptr<GDALDataset, decltype(&GDALClose)> ds_guard(ds, GDALClose);

        GDALRasterBand* band = ds->GetRasterBand(1);
        if (!band)
            return result;

        int xSize = band->GetXSize();
        int ySize = band->GetYSize();

        std::vector<GByte> data(xSize * ySize);
        band->RasterIO(GF_Read, 0, 0, xSize, ySize, data.data(), xSize, ySize, GDT_Byte, 0, 0);

        OGRLayer* polyLayer = nullptr;
        GDALDriver* memDriver = GetGDALDriverManager()->GetDriverByName("MEM");
        GDALDataset* memDs = memDriver ? memDriver->Create("", 0, 0, 0, GDT_Unknown, nullptr) : nullptr;
        if (memDs)
        {
            const OGRSpatialReference* srs = ds->GetSpatialRef();
            polyLayer = memDs->CreateLayer("features", srs, wkbPolygon, nullptr);
        }

        if (!polyLayer)
        {
            if (memDs)
                GDALClose(memDs);
            return result;
        }

        GDALRasterBand* tmpBand = nullptr;
        GDALDataset* tmpDs = memDriver ? memDriver->Create("", xSize, ySize, 1, GDT_Byte, nullptr) : nullptr;
        if (tmpDs)
        {
            tmpBand = tmpDs->GetRasterBand(1);
            tmpBand->RasterIO(GF_Write, 0, 0, xSize, ySize, data.data(), xSize, ySize, GDT_Byte, 0, 0);
        }

        if (tmpBand)
        {
            char** papszOptions = nullptr;
            papszOptions = CSLSetNameValue(papszOptions, "8CONNECTED", "8");
            GDALPolygonize(tmpBand, tmpBand, polyLayer, 0, papszOptions, nullptr, nullptr);
            CSLDestroy(papszOptions);
        }

        if (tmpDs)
            GDALClose(tmpDs);

        polyLayer->ResetReading();
        OGRFeature* feature;
        while ((feature = polyLayer->GetNextFeature()) != nullptr)
        {
            OGRGeometry* geom = feature->GetGeometryRef();
            if (geom && !geom->IsEmpty())
            {
                char* wkt = nullptr;
                geom->exportToWkt(&wkt);
                if (wkt)
                {
                    soilwatercm::models::Geometry g;
                    g.wkt = wkt;
                    g.srid = "EPSG:4326";
                    result.push_back(g);
                    CPLFree(wkt);
                }
            }
            OGRFeature::DestroyFeature(feature);
        }

        if (memDs)
            GDALClose(memDs);
        return result;
    }

}  // namespace soilwatercm::infrastructure::gdal
