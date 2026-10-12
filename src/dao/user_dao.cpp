#include "user_dao.h"

#include "utils/logger.h"

namespace soilwatercm::dao
{

    UserDao::UserDao(std::shared_ptr<repository::PostgresRepository> repo) : DaoBase<models::User>(repo) {}

    std::optional<models::User> UserDao::findById(int id)
    {
        std::string sql = buildSelectQuery("users",
                                           {"id", "username", "password_hash", "real_name", "email", "phone", "role",
                                            "is_active", "created_at", "last_login"},
                                           "id = " + std::to_string(id));

        auto result = repo_->executeQuery(sql);
        if (result.empty())
        {
            return std::nullopt;
        }

        return mapRowToUser(result[0]);
    }

    std::optional<models::User> UserDao::findByUsername(const std::string& username)
    {
        std::string sql = buildSelectQuery("users",
                                           {"id", "username", "password_hash", "real_name", "email", "phone", "role",
                                            "is_active", "created_at", "last_login"},
                                           "username = '" + escapeString(username) + "'");

        auto result = repo_->executeQuery(sql);
        if (result.empty())
        {
            return std::nullopt;
        }

        return mapRowToUser(result[0]);
    }

    std::vector<models::User> UserDao::list(int page, int pageSize, const std::string& orderBy, bool ascending)
    {
        std::string sql = buildSelectQuery("users", {"id", "username", "password_hash", "real_name", "email", "phone",
                                                     "role", "is_active", "created_at", "last_login"});

        sql += buildPagination(page, pageSize, orderBy, ascending);

        auto result = repo_->executeQuery(sql);
        std::vector<models::User> users;

        for (const auto& row : result)
        {
            users.push_back(mapRowToUser(row));
        }

        return users;
    }

    int UserDao::insert(const models::User& user)
    {
        std::string sql = buildInsertQuery(
            "users", {"username", "password_hash", "real_name", "email", "phone", "role", "is_active", "created_at"},
            {"'" + escapeString(user.username) + "'", "'" + escapeString(user.password_hash) + "'",
             "'" + escapeString(user.real_name) + "'", "'" + escapeString(user.email) + "'",
             user.phone ? "'" + escapeString(*user.phone) + "'" : "NULL", std::to_string(static_cast<int>(user.role)),
             user.is_active ? "TRUE" : "FALSE", "'" + timestampToString(user.created_at) + "'"});

        auto result = repo_->executeQuery(sql);
        if (result.empty() || result[0].find("id") == result[0].end())
        {
            return -1;
        }

        return std::stoi(result[0]["id"]);
    }

    bool UserDao::update(const models::User& user)
    {
        std::string sql = buildUpdateQuery(
            "users", {"username", "real_name", "email", "phone", "role", "is_active"},
            {"'" + escapeString(user.username) + "'", "'" + escapeString(user.real_name) + "'",
             "'" + escapeString(user.email) + "'", user.phone ? "'" + escapeString(*user.phone) + "'" : "NULL",
             std::to_string(static_cast<int>(user.role)), user.is_active ? "TRUE" : "FALSE"},
            "id = " + std::to_string(user.id));

        return repo_->executeUpdate(sql);
    }

    bool UserDao::remove(int id)
    {
        std::string sql = buildDeleteQuery("users", "id = " + std::to_string(id));
        return repo_->executeUpdate(sql);
    }

    int UserDao::count()
    {
        std::string sql = buildCountQuery("users");
        auto result = repo_->executeQuery(sql);
        if (result.empty())
        {
            return 0;
        }
        return std::stoi(result[0]["count"]);
    }

    bool UserDao::updateLastLogin(int userId)
    {
        auto now = std::chrono::system_clock::now();
        std::string sql =
            "UPDATE users SET last_login = '" + timestampToString(now) + "' WHERE id = " + std::to_string(userId);
        return repo_->executeUpdate(sql);
    }

    bool UserDao::existsByUsername(const std::string& username)
    {
        std::string sql = buildCountQuery("users", "username = '" + escapeString(username) + "'");
        auto result = repo_->executeQuery(sql);
        if (result.empty())
        {
            return false;
        }
        return std::stoi(result[0]["count"]) > 0;
    }

    std::vector<models::User> UserDao::findByRole(models::UserRole role)
    {
        std::string sql = buildSelectQuery("users",
                                           {"id", "username", "password_hash", "real_name", "email", "phone", "role",
                                            "is_active", "created_at", "last_login"},
                                           "role = " + std::to_string(static_cast<int>(role)));

        auto result = repo_->executeQuery(sql);
        std::vector<models::User> users;

        for (const auto& row : result)
        {
            users.push_back(mapRowToUser(row));
        }

        return users;
    }

    models::User UserDao::mapRowToUser(const std::map<std::string, std::string>& row)
    {
        models::User user;

        user.id = std::stoi(row.at("id"));
        user.username = row.at("username");
        user.password_hash = row.at("password_hash");
        user.real_name = row.at("real_name");
        user.email = row.at("email");

        if (row.find("phone") != row.end() && row.at("phone") != "NULL")
        {
            user.phone = row.at("phone");
        }

        user.role = static_cast<models::UserRole>(std::stoi(row.at("role")));
        user.is_active = (row.at("is_active") == "t" || row.at("is_active") == "TRUE");

        user.created_at = stringToTimestamp(row.at("created_at"));

        if (row.find("last_login") != row.end() && row.at("last_login") != "NULL")
        {
            user.last_login = stringToTimestamp(row.at("last_login"));
        }

        return user;
    }

}  // namespace soilwatercm::dao
