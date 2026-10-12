#define _USE_MATH_DEFINES
#include "spatial_ops.h"

#include <cmath>
#include <memory>

#include <gdal.h>
#include <ogr_geometry.h>
#include <ogr_spatialref.h>

namespace soilwatercm::infrastructure::gdal
{

    namespace
    {
        struct GeometryDeleter
        {
            void operator()(OGRGeometry* p) const { OGRGeometryFactory::destroyGeometry(p); }
        };
        using UniqueGeom = std::unique_ptr<OGRGeometry, GeometryDeleter>;

        UniqueGeom parseWkt(const std::string& wkt)
        {
            if (wkt.empty())
                return nullptr;
            OGRGeometry* geom = nullptr;
            OGRGeometryFactory::createFromWkt(wkt.c_str(), nullptr, &geom);
            return UniqueGeom(geom);
        }

        std::string toWkt(OGRGeometry* geom)
        {
            if (!geom)
                return "";
            char* wkt = nullptr;
            geom->exportToWkt(&wkt);
            std::string result(wkt ? wkt : "");
            if (wkt)
                CPLFree(wkt);
            return result;
        }
    }  // namespace

    SpatialOps::SpatialOps() {}

    double SpatialOps::distance(const Point& p1, const Point& p2)
    {
        return distance(p1.x, p1.y, p2.x, p2.y);
    }

    double SpatialOps::distance(double x1, double y1, double x2, double y2)
    {
        double dx = x2 - x1;
        double dy = y2 - y1;
        return std::sqrt(dx * dx + dy * dy);
    }

    double SpatialOps::haversineDistance(double lon1, double lat1, double lon2, double lat2)
    {
        const double R = 6371000.0;

        double lat1_rad = lat1 * M_PI / 180.0;
        double lat2_rad = lat2 * M_PI / 180.0;
        double delta_lat = (lat2 - lat1) * M_PI / 180.0;
        double delta_lon = (lon2 - lon1) * M_PI / 180.0;

        double a = std::sin(delta_lat / 2) * std::sin(delta_lat / 2) +
                   std::cos(lat1_rad) * std::cos(lat2_rad) * std::sin(delta_lon / 2) * std::sin(delta_lon / 2);

        double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));

        return R * c;
    }

    Point SpatialOps::project(double lon, double lat, const std::string& target_srs)
    {
        OGRSpatialReference srcSrs;
        srcSrs.importFromEPSG(4326);
        srcSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        OGRSpatialReference dstSrs;
        if (target_srs.find("EPSG:") == 0)
        {
            int epsg = std::stoi(target_srs.substr(5));
            dstSrs.importFromEPSG(epsg);
        }
        else
        {
            dstSrs.importFromProj4(target_srs.c_str());
        }
        dstSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        OGRCoordinateTransformation* transform = OGRCreateCoordinateTransformation(&srcSrs, &dstSrs);
        if (!transform)
        {
            return Point(lon, lat);
        }

        double x = lon, y = lat;
        transform->Transform(1, &x, &y);
        delete transform;

        return Point(x, y);
    }

    Point SpatialOps::inverseProject(double x, double y, const std::string& source_srs)
    {
        OGRSpatialReference srcSrs;
        if (source_srs.find("EPSG:") == 0)
        {
            int epsg = std::stoi(source_srs.substr(5));
            srcSrs.importFromEPSG(epsg);
        }
        else
        {
            srcSrs.importFromProj4(source_srs.c_str());
        }
        srcSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        OGRSpatialReference dstSrs;
        dstSrs.importFromEPSG(4326);
        dstSrs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        OGRCoordinateTransformation* transform = OGRCreateCoordinateTransformation(&srcSrs, &dstSrs);
        if (!transform)
        {
            return Point(x, y);
        }

        double lon = x, lat = y;
        transform->Transform(1, &lon, &lat);
        delete transform;

        return Point(lon, lat);
    }

    bool SpatialOps::intersects(const std::string& geom1_wkt, const std::string& geom2_wkt)
    {
        auto g1 = parseWkt(geom1_wkt);
        auto g2 = parseWkt(geom2_wkt);
        if (!g1 || !g2)
            return false;
        return g1->Intersects(g2.get());
    }

    bool SpatialOps::contains(const std::string& geom1_wkt, const std::string& geom2_wkt)
    {
        auto g1 = parseWkt(geom1_wkt);
        auto g2 = parseWkt(geom2_wkt);
        if (!g1 || !g2)
            return false;
        return g1->Contains(g2.get());
    }

    bool SpatialOps::within(const std::string& geom1_wkt, const std::string& geom2_wkt)
    {
        auto g1 = parseWkt(geom1_wkt);
        auto g2 = parseWkt(geom2_wkt);
        if (!g1 || !g2)
            return false;
        return g1->Within(g2.get());
    }

    bool SpatialOps::touches(const std::string& geom1_wkt, const std::string& geom2_wkt)
    {
        auto g1 = parseWkt(geom1_wkt);
        auto g2 = parseWkt(geom2_wkt);
        if (!g1 || !g2)
            return false;
        return g1->Touches(g2.get());
    }

    std::string SpatialOps::intersection(const std::string& geom1_wkt, const std::string& geom2_wkt)
    {
        auto g1 = parseWkt(geom1_wkt);
        auto g2 = parseWkt(geom2_wkt);
        if (!g1 || !g2)
            return "";
        OGRGeometry* result = g1->Intersection(g2.get());
        std::string wkt = toWkt(result);
        OGRGeometryFactory::destroyGeometry(result);
        return wkt;
    }

    std::string SpatialOps::union_(const std::string& geom1_wkt, const std::string& geom2_wkt)
    {
        auto g1 = parseWkt(geom1_wkt);
        auto g2 = parseWkt(geom2_wkt);
        if (!g1 || !g2)
            return "";
        OGRGeometry* result = g1->Union(g2.get());
        std::string wkt = toWkt(result);
        OGRGeometryFactory::destroyGeometry(result);
        return wkt;
    }

    std::string SpatialOps::difference(const std::string& geom1_wkt, const std::string& geom2_wkt)
    {
        auto g1 = parseWkt(geom1_wkt);
        auto g2 = parseWkt(geom2_wkt);
        if (!g1 || !g2)
            return "";
        OGRGeometry* result = g1->Difference(g2.get());
        std::string wkt = toWkt(result);
        OGRGeometryFactory::destroyGeometry(result);
        return wkt;
    }

    std::string SpatialOps::buffer(const std::string& geom_wkt, double distance)
    {
        auto g = parseWkt(geom_wkt);
        if (!g)
            return "";
        OGRGeometry* result = g->Buffer(distance);
        std::string wkt = toWkt(result);
        OGRGeometryFactory::destroyGeometry(result);
        return wkt;
    }

    double SpatialOps::area(const std::string& geom_wkt)
    {
        auto g = parseWkt(geom_wkt);
        if (!g)
            return 0.0;
        if (auto* cp = g->toCurvePolygon())
            return cp->get_Area();
        if (auto* mp = g->toMultiPolygon())
            return mp->get_Area();
        return 0.0;
    }

    double SpatialOps::length(const std::string& geom_wkt)
    {
        auto g = parseWkt(geom_wkt);
        if (!g)
            return 0.0;
        if (auto* c = g->toCurve())
            return c->get_Length();
        if (auto* mc = g->toMultiCurve())
            return mc->get_Length();
        return 0.0;
    }

    double SpatialOps::perimeter(const std::string& geom_wkt)
    {
        auto g = parseWkt(geom_wkt);
        if (!g)
            return 0.0;
        if (auto* c = g->toCurve())
            return c->get_Length();
        if (auto* mc = g->toMultiCurve())
            return mc->get_Length();
        return 0.0;
    }

    Point SpatialOps::centroid(const std::string& geom_wkt)
    {
        auto g = parseWkt(geom_wkt);
        if (!g)
            return Point(0.0, 0.0);

        OGRPoint c;
        if (g->Centroid(&c) == OGRERR_NONE)
        {
            return Point(c.getX(), c.getY());
        }
        return Point(0.0, 0.0);
    }

    std::string SpatialOps::simplify(const std::string& geom_wkt, double tolerance)
    {
        auto g = parseWkt(geom_wkt);
        if (!g)
            return "";
        OGRGeometry* result = g->Simplify(tolerance);
        std::string wkt = toWkt(result);
        OGRGeometryFactory::destroyGeometry(result);
        return wkt;
    }

    bool SpatialOps::isValid(const std::string& geom_wkt)
    {
        auto g = parseWkt(geom_wkt);
        if (!g)
            return false;
        return g->IsValid();
    }

    std::string SpatialOps::makeValid(const std::string& geom_wkt)
    {
        auto g = parseWkt(geom_wkt);
        if (!g)
            return "";
        if (g->IsValid())
            return geom_wkt;

        OGRGeometry* result = nullptr;
        char* options[] = {const_cast<char*>("MAKEVALID=YES"), nullptr};
        result = g->MakeValid();
        if (!result)
            return geom_wkt;

        std::string wkt = toWkt(result);
        OGRGeometryFactory::destroyGeometry(result);
        return wkt;
    }

}  // namespace soilwatercm::infrastructure::gdal
