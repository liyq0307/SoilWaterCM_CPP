#pragma once

#include <ctime>
#include <functional>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "../models/common/geo_types.h"
#include "../repository/postgres_repository.h"

namespace soilwatercm::dao
{

    /**
     * @brief DAO 模板基类
     *
     * 统一单表实体的增删改查接口, 并提供 SQL 拼接、分页、几何 WKT 互转、
     * 字符串转义与时间戳文本互转等辅助。
     */
    template <typename T>
    class DaoBase
    {
    public:
        /**
         * @brief 构造, 注入数据库仓库
         * @param repo PostgreSQL 访问仓库
         */
        explicit DaoBase(std::shared_ptr<repository::PostgresRepository> repo) : repo_(repo) {}

        /// @brief 虚析构, 支持多态删除
        virtual ~DaoBase() = default;

        /**
         * @brief 按主键查单条
         * @param id 主键
         * @return 记录; 不存在返回 nullopt
         */
        virtual std::optional<T> findById(int id) = 0;

        /**
         * @brief 分页列表
         * @param page 页码, 从 1 开始
         * @param pageSize 每页条数
         * @param orderBy 排序列名
         * @param ascending 是否升序
         * @return 当前页记录列表
         */
        virtual std::vector<T> list(int page = 1, int pageSize = 20, const std::string& orderBy = "id",
                                    bool ascending = true) = 0;

        /**
         * @brief 插入一条
         * @param entity 待插入实体
         * @return 新记录 id; 失败返回 -1
         */
        virtual int insert(const T& entity) = 0;

        /**
         * @brief 按 id 更新
         * @param entity 实体 (须含 id)
         * @return 更新成功返回 true
         */
        virtual bool update(const T& entity) = 0;

        /**
         * @brief 按 id 删除
         * @param id 主键
         * @return 删除成功返回 true
         */
        virtual bool remove(int id) = 0;

        /**
         * @brief 总条数
         * @return 表内记录数
         */
        virtual int count() = 0;

    protected:
        std::shared_ptr<repository::PostgresRepository> repo_;

        /// @brief 拼 SELECT, 可带 WHERE
        std::string buildSelectQuery(const std::string& table, const std::vector<std::string>& fields,
                                     const std::string& whereClause = "")
        {
            std::string sql = "SELECT ";
            for (size_t i = 0; i < fields.size(); ++i)
            {
                if (i > 0)
                    sql += ", ";
                sql += fields[i];
            }
            sql += " FROM " + table;
            if (!whereClause.empty())
            {
                sql += " WHERE " + whereClause;
            }
            return sql;
        }

        /// @brief 拼 INSERT ... RETURNING id
        std::string buildInsertQuery(const std::string& table, const std::vector<std::string>& fields,
                                     const std::vector<std::string>& placeholders)
        {
            std::string sql = "INSERT INTO " + table + " (";
            for (size_t i = 0; i < fields.size(); ++i)
            {
                if (i > 0)
                    sql += ", ";
                sql += fields[i];
            }
            sql += ") VALUES (";
            for (size_t i = 0; i < placeholders.size(); ++i)
            {
                if (i > 0)
                    sql += ", ";
                sql += placeholders[i];
            }
            sql += ") RETURNING id";
            return sql;
        }

        /// @brief 拼 UPDATE ... SET ... WHERE
        std::string buildUpdateQuery(const std::string& table, const std::vector<std::string>& fields,
                                     const std::vector<std::string>& placeholders, const std::string& whereClause)
        {
            std::string sql = "UPDATE " + table + " SET ";
            for (size_t i = 0; i < fields.size(); ++i)
            {
                if (i > 0)
                    sql += ", ";
                sql += fields[i] + " = " + placeholders[i];
            }
            sql += " WHERE " + whereClause;
            return sql;
        }

        /// @brief 拼 DELETE ... WHERE
        std::string buildDeleteQuery(const std::string& table, const std::string& whereClause)
        {
            return "DELETE FROM " + table + " WHERE " + whereClause;
        }

        /// @brief 拼 SELECT COUNT(*), 可带 WHERE
        std::string buildCountQuery(const std::string& table, const std::string& whereClause = "")
        {
            std::string sql = "SELECT COUNT(*) FROM " + table;
            if (!whereClause.empty())
            {
                sql += " WHERE " + whereClause;
            }
            return sql;
        }

        /// @brief 追加 ORDER BY 和 LIMIT/OFFSET
        std::string buildPagination(int page, int pageSize, const std::string& orderBy, bool ascending)
        {
            std::string sql = " ORDER BY " + orderBy;
            sql += (ascending ? " ASC" : " DESC");
            sql += " LIMIT " + std::to_string(pageSize);
            sql += " OFFSET " + std::to_string((page - 1) * pageSize);
            return sql;
        }

        /// @brief 几何对象 -> WKT 文本
        std::string geometryToWkt(const models::Geometry& geom) { return geom.wkt; }

        /// @brief WKT 文本 -> 几何对象, 默认 EPSG:4326
        models::Geometry wktToGeometry(const std::string& wkt, const std::string& srid = "EPSG:4326")
        {
            models::Geometry geom;
            geom.wkt = wkt;
            geom.srid = srid;
            return geom;
        }

        /// @brief 拼接 SQL 时转义单引号和反斜杠
        std::string escapeString(const std::string& str)
        {
            std::string result;
            result.reserve(str.size());
            for (char c : str)
            {
                if (c == '\'')
                {
                    result += "''";
                }
                else if (c == '\\')
                {
                    result += "\\\\";
                }
                else
                {
                    result += c;
                }
            }
            return result;
        }

        /// @brief 时间点 -> "YYYY-MM-DD HH:MM:SS" 文本
        std::string timestampToString(const std::chrono::system_clock::time_point& tp)
        {
            auto time = std::chrono::system_clock::to_time_t(tp);
            std::tm tm = {};
            localtime_s(&tm, &time);
            std::stringstream ss;
            ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
            return ss.str();
        }

        /// @brief "YYYY-MM-DD HH:MM:SS" 文本 -> 时间点
        std::chrono::system_clock::time_point stringToTimestamp(const std::string& str)
        {
            std::tm tm = {};
            std::istringstream ss(str);
            ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
            return std::chrono::system_clock::from_time_t(std::mktime(&tm));
        }
    };

}  // namespace soilwatercm::dao
