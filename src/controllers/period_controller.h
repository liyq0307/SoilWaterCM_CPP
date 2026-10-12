#pragma once

#include <drogon/HttpController.h>

#include "../services/period_service.h"

namespace soilwatercm::controllers
{

    /**
     * @brief 监测期管理 HTTP 控制器
     *
     * 提供期次 CRUD、状态流转与归档, 路由前缀 /api/v1/periods。
     */
    class PeriodController : public drogon::HttpController<PeriodController, false>
    {
    public:
        PeriodController() = default;

        /**
         * @brief 构造, 注入监测期服务
         * @param periodService 监测期服务
         */
        explicit PeriodController(std::shared_ptr<services::PeriodService> periodService);

        METHOD_LIST_BEGIN
        ADD_METHOD_TO(PeriodController::listPeriods, "/api/v1/periods", drogon::Get);
        ADD_METHOD_TO(PeriodController::createPeriod, "/api/v1/periods", drogon::Post);
        // 静态路径先于 /{id} 注册, 避免被参数路径吞掉
        ADD_METHOD_TO(PeriodController::listArchives, "/api/v1/periods/archives/list", drogon::Get);
        ADD_METHOD_TO(PeriodController::getPeriod, "/api/v1/periods/{id}", drogon::Get);
        ADD_METHOD_TO(PeriodController::updateStatus, "/api/v1/periods/{id}/status", drogon::Put);
        ADD_METHOD_TO(PeriodController::archivePeriod, "/api/v1/periods/{id}/archive", drogon::Post);
        ADD_METHOD_TO(PeriodController::getArchiveManifest, "/api/v1/periods/{id}/archive-manifest", drogon::Get);
        METHOD_LIST_END

        /**
         * @brief GET /api/v1/periods?project_id= 列出监测期
         * @return 通过 callback 返回监测期列表; project_id 为空则返回全部
         */
        void listPeriods(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/periods 创建监测期
         *
         * body 含类型、起止日期与状态。
         * @return 通过 callback 返回 201 与新记录
         */
        void createPeriod(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/periods/{id} 查询监测期详情
         * @param id 监测期 id
         * @return 通过 callback 返回监测期记录
         */
        void getPeriod(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief PUT /api/v1/periods/{id}/status 更新监测期状态
         * @param id 监测期 id
         * @return 通过 callback 返回更新后的记录; 不存在返回 404
         */
        void updateStatus(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief GET /api/v1/periods/archives/list?project_id= 列出已归档监测期
         * @return 通过 callback 返回归档列表
         */
        void listArchives(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/periods/{id}/archive 归档监测期成果
         * @param id 监测期 id
         * @return 通过 callback 返回归档清单
         */
        void archivePeriod(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief GET /api/v1/periods/{id}/archive-manifest 读取归档清单
         * @param id 监测期 id
         * @return 通过 callback 返回归档清单; 未归档返回 404
         */
        void getArchiveManifest(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

    private:
        std::shared_ptr<services::PeriodService> periodService_;
    };

}  // namespace soilwatercm::controllers
