#include "basemap_dao.h"

#include "utils/logger.h"

namespace soilwatercm::dao
{

    namespace
    {
        const char* const kColumns[] = {"id",
                                        "project_id",
                                        "name",
                                        "file_path",
                                        "file_size_mb",
                                        "width_px",
                                        "height_px",
                                        "crs",
                                        "ST_AsText(footprint) as footprint",
                                        "band_count",
                                        "stretch_min",
                                        "stretch_max",
                                        "wms_layer_name",
                                        "note",
                                        "created_at"};

        std::vector<std::string> columnList()
        {
            return std::vector<std::string>(kColumns, kColumns + sizeof(kColumns) / sizeof(kColumns[0]));
        }

        std::string optIntToSql(const std::optional<int>& v)
        {
            return v.has_value() ? std::to_string(*v) : "NULL";
        }

        std::string optDoubleToSql(const std::optional<double>& v)
        {
            return v.has_value() ? std::to_string(*v) : "NULL";
        }

        std::optional<double> parseOptDouble(const std::map<std::string, std::string>& row, const std::string& key)
        {
            auto it = row.find(key);
            if (it == row.end() || it->second.empty() || it->second == "NULL")
                return std::nullopt;
            try
            {
                return std::stod(it->second);
            }
            catch (const std::exception&)
            {
                return std::nullopt;
            }
        }

        std::optional<int> parseOptInt(const std::map<std::string, std::string>& row, const std::string& key)
        {
            auto it = row.find(key);
            if (it == row.end() || it->second.empty() || it->second == "NULL")
                return std::nullopt;
            try
            {
                return std::stoi(it->second);
            }
            catch (const std::exception&)
            {
                return std::nullopt;
            }
        }

        std::optional<std::string> parseOptString(const std::map<std::string, std::string>& row, const std::string& key)
        {
            auto it = row.find(key);
            if (it == row.end() || it->second.empty() || it->second == "NULL")
                return std::nullopt;
            return it->second;
        }

        std::string parseString(const std::map<std::string, std::string>& row, const std::string& key)
        {
            auto it = row.find(key);
            if (it == row.end() || it->second == "NULL")
                return "";
            return it->second;
        }
    }  // namespace

    BasemapDao::BasemapDao(std::shared_ptr<repository::PostgresRepository> repo) : DaoBase<models::BasemapImage>(repo)
    {
    }

    std::optional<models::BasemapImage> BasemapDao::findById(int id)
    {
        std::string sql = buildSelectQuery("basemap_images", columnList(), "id = " + std::to_string(id));
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        return mapRow(result[0]);
    }

    std::vector<models::BasemapImage> BasemapDao::list(int page, int pageSize, const std::string& orderBy,
                                                       bool ascending)
    {
        std::string sql = buildSelectQuery("basemap_images", columnList());
        sql += buildPagination(page, pageSize, orderBy, ascending);
        auto result = repo_->executeQuery(sql);
        std::vector<models::BasemapImage> rows;
        for (const auto& r : result)
            rows.push_back(mapRow(r));
        return rows;
    }

    std::vector<models::BasemapImage> BasemapDao::findByProject(int projectId, int page, int pageSize)
    {
        std::string sql = buildSelectQuery("basemap_images", columnList(), "project_id = " + std::to_string(projectId));
        sql += buildPagination(page, pageSize, "id", true);
        auto result = repo_->executeQuery(sql);
        std::vector<models::BasemapImage> rows;
        for (const auto& r : result)
            rows.push_back(mapRow(r));
        return rows;
    }

    int BasemapDao::insert(const models::BasemapImage& basemap)
    {
        std::string footprintSql = basemap.footprint.wkt.empty()
                                       ? "NULL"
                                       : "ST_GeomFromText('" + escapeString(basemap.footprint.wkt) + "', 4326)";

        std::string sql =
            "INSERT INTO basemap_images (project_id, name, file_path, file_size_mb, width_px, height_px, crs, "
            "footprint, band_count, stretch_min, stretch_max, wms_layer_name, note, created_at) VALUES (" +
            std::to_string(basemap.project_id) + ", '" + escapeString(basemap.name) + "', '" +
            escapeString(basemap.file_path) + "', " + optDoubleToSql(basemap.file_size_mb) + ", " +
            optIntToSql(basemap.width_px) + ", " + optIntToSql(basemap.height_px) + ", '" + escapeString(basemap.crs) +
            "', " + footprintSql + ", " + optIntToSql(basemap.band_count) + ", " + optDoubleToSql(basemap.stretch_min) +
            ", " + optDoubleToSql(basemap.stretch_max) + ", " +
            (basemap.wms_layer_name.has_value() ? "'" + escapeString(*basemap.wms_layer_name) + "'" : "NULL") + ", " +
            (basemap.note.empty() ? "NULL" : "'" + escapeString(basemap.note) + "'") + ", '" +
            timestampToString(basemap.created_at) + "') RETURNING id";
        auto result = repo_->executeQuery(sql);
        return result.empty() ? -1 : std::stoi(result[0]["id"]);
    }

    bool BasemapDao::update(const models::BasemapImage& basemap)
    {
        std::string footprintSql = basemap.footprint.wkt.empty()
                                       ? "NULL"
                                       : "ST_GeomFromText('" + escapeString(basemap.footprint.wkt) + "', 4326)";

        std::string sql =
            "UPDATE basemap_images SET project_id = " + std::to_string(basemap.project_id) + ", name = '" +
            escapeString(basemap.name) + "', file_path = '" + escapeString(basemap.file_path) +
            "', file_size_mb = " + optDoubleToSql(basemap.file_size_mb) +
            ", width_px = " + optIntToSql(basemap.width_px) + ", height_px = " + optIntToSql(basemap.height_px) +
            ", crs = '" + escapeString(basemap.crs) + "', footprint = " + footprintSql +
            ", band_count = " + optIntToSql(basemap.band_count) +
            ", stretch_min = " + optDoubleToSql(basemap.stretch_min) +
            ", stretch_max = " + optDoubleToSql(basemap.stretch_max) + ", wms_layer_name = " +
            (basemap.wms_layer_name.has_value() ? "'" + escapeString(*basemap.wms_layer_name) + "'" : "NULL") +
            ", note = " + (basemap.note.empty() ? "NULL" : "'" + escapeString(basemap.note) + "'") +
            " WHERE id = " + std::to_string(basemap.id);
        return repo_->executeUpdate(sql);
    }

    bool BasemapDao::remove(int id)
    {
        std::string sql = buildDeleteQuery("basemap_images", "id = " + std::to_string(id));
        return repo_->executeUpdate(sql);
    }

    int BasemapDao::count()
    {
        std::string sql = buildCountQuery("basemap_images");
        auto result = repo_->executeQuery(sql);
        return result.empty() ? 0 : std::stoi(result[0]["count"]);
    }

    models::BasemapImage BasemapDao::mapRow(const std::map<std::string, std::string>& row)
    {
        models::BasemapImage b;
        b.id = std::stoi(row.at("id"));
        b.project_id = std::stoi(row.at("project_id"));
        b.name = parseString(row, "name");
        b.file_path = parseString(row, "file_path");
        b.file_size_mb = parseOptDouble(row, "file_size_mb");
        b.width_px = parseOptInt(row, "width_px");
        b.height_px = parseOptInt(row, "height_px");
        b.crs = parseString(row, "crs");
        if (b.crs.empty())
            b.crs = "EPSG:4326";
        std::string fp = parseString(row, "footprint");
        if (!fp.empty())
        {
            b.footprint = wktToGeometry(fp);
        }
        b.band_count = parseOptInt(row, "band_count");
        b.stretch_min = parseOptDouble(row, "stretch_min");
        b.stretch_max = parseOptDouble(row, "stretch_max");
        b.wms_layer_name = parseOptString(row, "wms_layer_name");
        b.note = parseString(row, "note");
        auto it = row.find("created_at");
        if (it != row.end() && !it->second.empty() && it->second != "NULL")
            b.created_at = stringToTimestamp(it->second);
        return b;
    }

}  // namespace soilwatercm::dao
