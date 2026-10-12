#include "geojson_utils.h"

#include <cmath>
#include <sstream>
#include <vector>

namespace soilwatercm::utils
{

    namespace
    {
        constexpr double kPi = 3.14159265358979323846;
        constexpr double kEarthRadiusM = 6378137.0;  // WGS84 赤道半径

        std::string trim(const std::string& s)
        {
            size_t b = s.find_first_not_of(" \t\r\n");
            if (b == std::string::npos)
                return "";
            size_t e = s.find_last_not_of(" \t\r\n");
            return s.substr(b, e - b + 1);
        }

        // 解析坐标串 "x y[,z] x y[,z] ..." -> 点列表
        std::vector<std::vector<double>> parseCoordList(const std::string& s)
        {
            std::vector<std::vector<double>> pts;
            std::istringstream sst(s);
            std::string token;
            std::vector<double> cur;
            while (sst >> token)
            {
                if (token == ",")
                {
                    if (!cur.empty())
                    {
                        pts.push_back(cur);
                        cur.clear();
                    }
                    continue;
                }
                try
                {
                    cur.push_back(std::stod(token));
                }
                catch (const std::exception&)
                {
                    cur.clear();
                }
            }
            if (!cur.empty())
                pts.push_back(cur);
            return pts;
        }

        // 解析环 "(( x y, x y ))" 内部
        std::vector<std::vector<std::vector<double>>> parseRings(const std::string& inner)
        {
            std::vector<std::vector<std::vector<double>>> rings;
            size_t pos = 0;
            while (true)
            {
                size_t open = inner.find("(", pos);
                if (open == std::string::npos)
                    break;
                size_t close = inner.find(")", open);
                if (close == std::string::npos)
                    break;
                std::string seg = trim(inner.substr(open + 1, close - open - 1));
                auto pts = parseCoordList(seg);
                if (pts.size() >= 3)
                    rings.push_back(pts);
                pos = close + 1;
            }
            return rings;
        }

        nlohmann::json ringToJson(const std::vector<std::vector<double>>& ring)
        {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& p : ring)
            {
                if (p.size() >= 3)
                    arr.push_back({p[0], p[1], p[2]});
                else
                    arr.push_back({p[0], p[1]});
            }
            return arr;
        }

        // 球面多边形面积 (平方米, 绝对值)
        double sphericalRingArea(const std::vector<std::vector<double>>& ring)
        {
            if (ring.size() < 3)
                return 0.0;
            double total = 0.0;
            size_t n = ring.size();
            for (size_t i = 0; i < n; ++i)
            {
                const auto& p1 = ring[i];
                const auto& p2 = ring[(i + 1) % n];
                double lon1 = p1[0] * kPi / 180.0;
                double lat1 = p1[1] * kPi / 180.0;
                double lon2 = p2[0] * kPi / 180.0;
                double lat2 = p2[1] * kPi / 180.0;
                total += (lon2 - lon1) * (2.0 + std::sin(lat1) + std::sin(lat2));
            }
            return std::abs(total * kEarthRadiusM * kEarthRadiusM / 2.0);
        }
    }  // namespace

    nlohmann::json GeoJsonUtils::wktToGeoJson(const std::string& wkt)
    {
        std::string s = trim(wkt);
        if (s.empty())
            return nullptr;

        std::string upper;
        upper.reserve(s.size());
        for (char c : s)
            upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));

        try
        {
            if (upper.rfind("POINT", 0) == 0)
            {
                size_t open = s.find('(');
                size_t close = s.find(')');
                if (open == std::string::npos || close == std::string::npos)
                    return nullptr;
                auto pts = parseCoordList(s.substr(open + 1, close - open - 1));
                if (pts.empty())
                    return nullptr;
                nlohmann::json j;
                j["type"] = "Point";
                if (pts[0].size() >= 3)
                    j["coordinates"] = {pts[0][0], pts[0][1], pts[0][2]};
                else
                    j["coordinates"] = {pts[0][0], pts[0][1]};
                return j;
            }
            if (upper.rfind("MULTIPOLYGON", 0) == 0)
            {
                size_t open = s.find('(');
                if (open == std::string::npos)
                    return nullptr;
                std::string body = s.substr(open);
                nlohmann::json polys = nlohmann::json::array();
                size_t pos = 0;
                while (true)
                {
                    size_t polyOpen = body.find("((", pos);
                    if (polyOpen == std::string::npos)
                        break;
                    // 找到匹配的 "))"
                    size_t depth = 0;
                    size_t i = polyOpen;
                    size_t polyClose = std::string::npos;
                    for (; i < body.size(); ++i)
                    {
                        if (body[i] == '(')
                            ++depth;
                        else if (body[i] == ')')
                        {
                            --depth;
                            if (depth == 0)
                            {
                                polyClose = i;
                                break;
                            }
                        }
                    }
                    if (polyClose == std::string::npos)
                        break;
                    std::string seg = body.substr(polyOpen, polyClose - polyOpen + 1);
                    auto rings = parseRings(seg);
                    if (!rings.empty())
                    {
                        nlohmann::json ringsJson = nlohmann::json::array();
                        for (const auto& r : rings)
                            ringsJson.push_back(ringToJson(r));
                        polys.push_back(ringsJson);
                    }
                    pos = polyClose + 1;
                }
                nlohmann::json j;
                j["type"] = "MultiPolygon";
                j["coordinates"] = polys;
                return j;
            }
            if (upper.rfind("POLYGON", 0) == 0)
            {
                size_t open = s.find('(');
                if (open == std::string::npos)
                    return nullptr;
                auto rings = parseRings(s.substr(open));
                nlohmann::json j;
                j["type"] = "Polygon";
                nlohmann::json ringsJson = nlohmann::json::array();
                for (const auto& r : rings)
                    ringsJson.push_back(ringToJson(r));
                j["coordinates"] = ringsJson;
                return j;
            }
        }
        catch (const std::exception&)
        {
            return nullptr;
        }

        return nullptr;
    }

    std::string GeoJsonUtils::geoJsonToWkt(const nlohmann::json& geometry)
    {
        if (!geometry.is_object() || !geometry.contains("type") || !geometry.contains("coordinates"))
            return "";

        std::string type = geometry["type"].get<std::string>();
        const auto& coords = geometry["coordinates"];

        auto coordStr = [](const nlohmann::json& c) -> std::string
        {
            if (!c.is_array() || c.size() < 2)
                return "";
            std::ostringstream oss;
            oss << c[0].get<double>() << " " << c[1].get<double>();
            if (c.size() >= 3 && c[2].is_number())
                oss << " " << c[2].get<double>();
            return oss.str();
        };

        if (type == "Point")
        {
            std::string c = coordStr(coords);
            if (c.empty())
                return "";
            return "POINT(" + c + ")";
        }
        if (type == "Polygon")
        {
            std::ostringstream oss;
            oss << "POLYGON(";
            for (size_t r = 0; r < coords.size(); ++r)
            {
                if (r > 0)
                    oss << ",";
                oss << "(";
                const auto& ring = coords[r];
                for (size_t i = 0; i < ring.size(); ++i)
                {
                    if (i > 0)
                        oss << ",";
                    oss << coordStr(ring[i]);
                }
                oss << ")";
            }
            oss << ")";
            return oss.str();
        }
        if (type == "MultiPolygon")
        {
            std::ostringstream oss;
            oss << "MULTIPOLYGON(";
            for (size_t p = 0; p < coords.size(); ++p)
            {
                if (p > 0)
                    oss << ",";
                oss << "(";
                const auto& poly = coords[p];
                for (size_t r = 0; r < poly.size(); ++r)
                {
                    if (r > 0)
                        oss << ",";
                    oss << "(";
                    const auto& ring = poly[r];
                    for (size_t i = 0; i < ring.size(); ++i)
                    {
                        if (i > 0)
                            oss << ",";
                        oss << coordStr(ring[i]);
                    }
                    oss << ")";
                }
                oss << ")";
            }
            oss << ")";
            return oss.str();
        }
        return "";
    }

    nlohmann::json GeoJsonUtils::makeFeatureCollection(const nlohmann::json& features)
    {
        nlohmann::json fc;
        fc["type"] = "FeatureCollection";
        fc["features"] = features;
        return fc;
    }

    nlohmann::json GeoJsonUtils::makeFeature(const nlohmann::json& geometry, const nlohmann::json& properties)
    {
        nlohmann::json f;
        f["type"] = "Feature";
        f["geometry"] = geometry;
        f["properties"] = properties;
        return f;
    }

    double GeoJsonUtils::areaHa(const std::string& wkt)
    {
        std::string s = trim(wkt);
        if (s.empty())
            return -1.0;

        std::string upper;
        for (char c : s)
            upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));

        try
        {
            double areaM2 = 0.0;
            if (upper.rfind("MULTIPOLYGON", 0) == 0)
            {
                size_t pos = 0;
                while (true)
                {
                    size_t polyOpen = s.find("((", pos);
                    if (polyOpen == std::string::npos)
                        break;
                    size_t depth = 0;
                    size_t i = polyOpen;
                    size_t polyClose = std::string::npos;
                    for (; i < s.size(); ++i)
                    {
                        if (s[i] == '(')
                            ++depth;
                        else if (s[i] == ')')
                        {
                            --depth;
                            if (depth == 0)
                            {
                                polyClose = i;
                                break;
                            }
                        }
                    }
                    if (polyClose == std::string::npos)
                        break;
                    auto rings = parseRings(s.substr(polyOpen, polyClose - polyOpen + 1));
                    double polyArea = rings.empty() ? 0.0 : sphericalRingArea(rings[0]);
                    for (size_t r = 1; r < rings.size(); ++r)
                        polyArea -= sphericalRingArea(rings[r]);
                    areaM2 += std::abs(polyArea);
                    pos = polyClose + 1;
                }
            }
            else if (upper.rfind("POLYGON", 0) == 0)
            {
                size_t open = s.find('(');
                auto rings = parseRings(s.substr(open));
                double polyArea = rings.empty() ? 0.0 : sphericalRingArea(rings[0]);
                for (size_t r = 1; r < rings.size(); ++r)
                    polyArea -= sphericalRingArea(rings[r]);
                areaM2 = std::abs(polyArea);
            }
            else
            {
                return -1.0;
            }
            return areaM2 / 10000.0;
        }
        catch (const std::exception&)
        {
            return -1.0;
        }
    }

}  // namespace soilwatercm::utils
