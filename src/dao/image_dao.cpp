#include "image_dao.h"

namespace soilwatercm::dao
{

    ImageDao::ImageDao(std::shared_ptr<repository::PostgresRepository> repo) : DaoBase<models::Image>(repo) {}

    std::optional<models::Image> ImageDao::findById(int id)
    {
        std::string sql =
            buildSelectQuery("images",
                             {"id", "image_code", "type", "sensor_type", "acquisition_time",
                              "ST_AsText(extent) as extent", "crs", "resolution", "file_path", "file_size",
                              "cloud_cover", "quality_status", "quality_message", "project_id", "period_id"},
                             "id = " + std::to_string(id));
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        return mapRowToImage(result[0]);
    }

    std::vector<models::Image> ImageDao::findByProjectId(int projectId)
    {
        std::string sql =
            buildSelectQuery("images",
                             {"id", "image_code", "type", "sensor_type", "acquisition_time",
                              "ST_AsText(extent) as extent", "crs", "resolution", "file_path", "file_size",
                              "cloud_cover", "quality_status", "quality_message", "project_id", "period_id"},
                             "project_id = " + std::to_string(projectId));
        auto result = repo_->executeQuery(sql);
        std::vector<models::Image> images;
        for (const auto& row : result)
            images.push_back(mapRowToImage(row));
        return images;
    }

    std::vector<models::Image> ImageDao::findByPeriodId(int periodId)
    {
        std::string sql =
            buildSelectQuery("images",
                             {"id", "image_code", "type", "sensor_type", "acquisition_time",
                              "ST_AsText(extent) as extent", "crs", "resolution", "file_path", "file_size",
                              "cloud_cover", "quality_status", "quality_message", "project_id", "period_id"},
                             "period_id = " + std::to_string(periodId));
        auto result = repo_->executeQuery(sql);
        std::vector<models::Image> images;
        for (const auto& row : result)
            images.push_back(mapRowToImage(row));
        return images;
    }

    std::vector<models::Image> ImageDao::findByQualityStatus(models::QualityStatus status)
    {
        std::string sql =
            buildSelectQuery("images",
                             {"id", "image_code", "type", "sensor_type", "acquisition_time",
                              "ST_AsText(extent) as extent", "crs", "resolution", "file_path", "file_size",
                              "cloud_cover", "quality_status", "quality_message", "project_id", "period_id"},
                             "quality_status = " + std::to_string(static_cast<int>(status)));
        auto result = repo_->executeQuery(sql);
        std::vector<models::Image> images;
        for (const auto& row : result)
            images.push_back(mapRowToImage(row));
        return images;
    }

    std::vector<models::Image> ImageDao::findByProjectAndPeriod(int projectId, int periodId)
    {
        std::string sql = buildSelectQuery(
            "images",
            {"id", "image_code", "type", "sensor_type", "acquisition_time", "ST_AsText(extent) as extent", "crs",
             "resolution", "file_path", "file_size", "cloud_cover", "quality_status", "quality_message", "project_id",
             "period_id"},
            "project_id = " + std::to_string(projectId) + " AND period_id = " + std::to_string(periodId));
        auto result = repo_->executeQuery(sql);
        std::vector<models::Image> images;
        for (const auto& row : result)
            images.push_back(mapRowToImage(row));
        return images;
    }

    std::vector<models::Image> ImageDao::findLatestSar(int projectId, int limit)
    {
        // type = 1 (ImageType::Sar), 按采集时间倒序取最近 N 期供 InSAR 主/从选择
        std::string sql = buildSelectQuery(
            "images",
            {"id", "image_code", "type", "sensor_type", "acquisition_time", "ST_AsText(extent) as extent", "crs",
             "resolution", "file_path", "file_size", "cloud_cover", "quality_status", "quality_message", "project_id",
             "period_id"},
            "project_id = " + std::to_string(projectId) + " AND type = " +
            std::to_string(static_cast<int>(models::ImageType::Sar)));
        sql += " ORDER BY acquisition_time DESC LIMIT " + std::to_string(limit);
        auto result = repo_->executeQuery(sql);
        std::vector<models::Image> images;
        for (const auto& row : result)
            images.push_back(mapRowToImage(row));
        return images;
    }

    std::vector<models::Image> ImageDao::list(int page, int pageSize, const std::string& orderBy, bool ascending)
    {
        std::string sql =
            buildSelectQuery("images", {"id", "image_code", "type", "sensor_type", "acquisition_time",
                                        "ST_AsText(extent) as extent", "crs", "resolution", "file_path", "file_size",
                                        "cloud_cover", "quality_status", "quality_message", "project_id", "period_id"});
        sql += buildPagination(page, pageSize, orderBy, ascending);
        auto result = repo_->executeQuery(sql);
        std::vector<models::Image> images;
        for (const auto& row : result)
            images.push_back(mapRowToImage(row));
        return images;
    }

    int ImageDao::insert(const models::Image& image)
    {
        std::string sql =
            "INSERT INTO images (image_code, type, sensor_type, acquisition_time, extent, crs, resolution, file_path, "
            "file_size, cloud_cover, quality_status, quality_message, project_id, period_id) VALUES ('" +
            escapeString(image.image_code) + "', " + std::to_string(static_cast<int>(image.type)) + ", '" +
            escapeString(image.sensor_type) + "', '" + timestampToString(image.acquisition_time) + "', " +
            "ST_GeomFromText('" + escapeString(image.extent.wkt) + "', 4326), '" + image.crs + "', " +
            std::to_string(image.resolution) + ", '" + escapeString(image.file_path) + "', " +
            std::to_string(image.file_size) + ", " + (image.cloud_cover ? std::to_string(*image.cloud_cover) : "NULL") +
            ", " + std::to_string(static_cast<int>(image.quality_status)) + ", " +
            (image.quality_message ? "'" + escapeString(*image.quality_message) + "'" : "NULL") + ", " +
            std::to_string(image.project_id) + ", " + std::to_string(image.period_id) + ") RETURNING id";
        auto result = repo_->executeQuery(sql);
        return result.empty() ? -1 : std::stoi(result[0]["id"]);
    }

    bool ImageDao::update(const models::Image& image)
    {
        std::string sql =
            "UPDATE images SET quality_status = " + std::to_string(static_cast<int>(image.quality_status));
        if (image.quality_message)
        {
            sql += ", quality_message = '" + escapeString(*image.quality_message) + "'";
        }
        sql += " WHERE id = " + std::to_string(image.id);
        return repo_->executeUpdate(sql);
    }

    bool ImageDao::remove(int id)
    {
        std::string sql = buildDeleteQuery("images", "id = " + std::to_string(id));
        return repo_->executeUpdate(sql);
    }

    int ImageDao::count()
    {
        std::string sql = buildCountQuery("images");
        auto result = repo_->executeQuery(sql);
        return result.empty() ? 0 : std::stoi(result[0]["count"]);
    }

    bool ImageDao::updateQualityStatus(int id, models::QualityStatus status, const std::string& message)
    {
        std::string sql = "UPDATE images SET quality_status = " + std::to_string(static_cast<int>(status)) +
                          ", quality_message = '" + escapeString(message) + "' WHERE id = " + std::to_string(id);
        return repo_->executeUpdate(sql);
    }

    models::Image ImageDao::mapRowToImage(const std::map<std::string, std::string>& row)
    {
        models::Image image;
        image.id = std::stoi(row.at("id"));
        image.image_code = row.at("image_code");
        image.type = static_cast<models::ImageType>(std::stoi(row.at("type")));
        image.sensor_type = row.at("sensor_type");
        image.acquisition_time = stringToTimestamp(row.at("acquisition_time"));
        if (row.find("extent") != row.end())
        {
            image.extent = wktToGeometry(row.at("extent"));
        }
        image.crs = row.at("crs");
        image.resolution = std::stod(row.at("resolution"));
        image.file_path = row.at("file_path");
        image.file_size = std::stoull(row.at("file_size"));
        if (row.find("cloud_cover") != row.end() && row.at("cloud_cover") != "NULL")
        {
            image.cloud_cover = std::stod(row.at("cloud_cover"));
        }
        image.quality_status = static_cast<models::QualityStatus>(std::stoi(row.at("quality_status")));
        if (row.find("quality_message") != row.end() && row.at("quality_message") != "NULL")
        {
            image.quality_message = row.at("quality_message");
        }
        image.project_id = std::stoi(row.at("project_id"));
        image.period_id = std::stoi(row.at("period_id"));
        return image;
    }

}  // namespace soilwatercm::dao
