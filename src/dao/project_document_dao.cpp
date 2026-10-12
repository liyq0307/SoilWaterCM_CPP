#include "project_document_dao.h"

#include "utils/logger.h"

namespace soilwatercm::dao
{

    namespace
    {
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

    ProjectDocumentDao::ProjectDocumentDao(std::shared_ptr<repository::PostgresRepository> repo)
        : DaoBase<models::ProjectDocument>(repo)
    {
    }

    std::optional<models::ProjectDocument> ProjectDocumentDao::findById(int id)
    {
        std::string sql = buildSelectQuery("project_documents",
                                           {"id", "project_id", "doc_type", "title", "name", "file_path",
                                            "file_size_kb", "uploaded_at"},
                                           "id = " + std::to_string(id));
        auto result = repo_->executeQuery(sql);
        if (result.empty())
            return std::nullopt;
        return mapRow(result[0]);
    }

    std::vector<models::ProjectDocument> ProjectDocumentDao::list(int page, int pageSize, const std::string& orderBy,
                                                                  bool ascending)
    {
        std::string sql = buildSelectQuery("project_documents",
                                           {"id", "project_id", "doc_type", "title", "name", "file_path", "file_size_kb",
                                            "uploaded_at"});
        sql += buildPagination(page, pageSize, orderBy, ascending);
        auto result = repo_->executeQuery(sql);
        std::vector<models::ProjectDocument> rows;
        for (const auto& r : result)
            rows.push_back(mapRow(r));
        return rows;
    }

    std::vector<models::ProjectDocument> ProjectDocumentDao::findByProject(int projectId)
    {
        std::string sql =
            buildSelectQuery("project_documents",
                             {"id", "project_id", "doc_type", "title", "name", "file_path", "file_size_kb",
                              "uploaded_at"},
                             "project_id = " + std::to_string(projectId));
        sql += " ORDER BY uploaded_at DESC";
        auto result = repo_->executeQuery(sql);
        std::vector<models::ProjectDocument> rows;
        for (const auto& r : result)
            rows.push_back(mapRow(r));
        return rows;
    }

    int ProjectDocumentDao::insert(const models::ProjectDocument& doc)
    {
        std::string sql =
            "INSERT INTO project_documents (project_id, doc_type, title, name, file_path, file_size_kb, uploaded_at) "
            "VALUES (" +
            std::to_string(doc.project_id) + ", '" + escapeString(doc.doc_type) + "', " +
            (doc.title.has_value() ? "'" + escapeString(*doc.title) + "'" : "NULL") + ", '" +
            escapeString(doc.name) + "', '" + escapeString(doc.file_path) + "', " + optDoubleToSql(doc.file_size_kb) +
            ", '" + timestampToString(doc.uploaded_at) + "') RETURNING id";
        auto result = repo_->executeQuery(sql);
        return result.empty() ? -1 : std::stoi(result[0]["id"]);
    }

    bool ProjectDocumentDao::update(const models::ProjectDocument& doc)
    {
        std::string sql = "UPDATE project_documents SET project_id = " + std::to_string(doc.project_id) +
                          ", doc_type = '" + escapeString(doc.doc_type) + "', title = " +
                          (doc.title.has_value() ? "'" + escapeString(*doc.title) + "'" : "NULL") + ", name = '" +
                          escapeString(doc.name) + "', file_path = '" + escapeString(doc.file_path) +
                          "', file_size_kb = " + optDoubleToSql(doc.file_size_kb) + ", uploaded_at = '" +
                          timestampToString(doc.uploaded_at) + "' WHERE id = " + std::to_string(doc.id);
        return repo_->executeUpdate(sql);
    }

    bool ProjectDocumentDao::remove(int id)
    {
        std::string sql = buildDeleteQuery("project_documents", "id = " + std::to_string(id));
        return repo_->executeUpdate(sql);
    }

    int ProjectDocumentDao::count()
    {
        std::string sql = buildCountQuery("project_documents");
        auto result = repo_->executeQuery(sql);
        return result.empty() ? 0 : std::stoi(result[0]["count"]);
    }

    models::ProjectDocument ProjectDocumentDao::mapRow(const std::map<std::string, std::string>& row)
    {
        models::ProjectDocument doc;
        doc.id = std::stoi(row.at("id"));
        doc.project_id = std::stoi(row.at("project_id"));
        doc.doc_type = parseString(row, "doc_type");
        doc.title = parseOptString(row, "title");
        doc.name = parseString(row, "name");
        doc.file_path = parseString(row, "file_path");
        doc.file_size_kb = parseOptDouble(row, "file_size_kb");
        doc.uploaded_at = stringToTimestamp(parseString(row, "uploaded_at"));
        return doc;
    }

}  // namespace soilwatercm::dao
