#pragma once

#include <drogon/HttpController.h>

#include "../services/report_service.h"

namespace soilwatercm::controllers
{

    /**
     * @brief 监测报告 HTTP 控制器
     *
     * 提供报告生成/查询/下载/归档与审核意见, 路由前缀 /api/v1/reports。
     */
    class ReportController : public drogon::HttpController<ReportController, false>
    {
    public:
        ReportController() = default;

        /**
         * @brief 构造, 注入报告服务
         * @param reportService 监测报告服务
         */
        explicit ReportController(std::shared_ptr<services::ReportService> reportService);

        METHOD_LIST_BEGIN
        ADD_METHOD_TO(ReportController::listReports, "/api/v1/reports", drogon::Get);
        ADD_METHOD_TO(ReportController::generate, "/api/v1/reports", drogon::Post);
        ADD_METHOD_TO(ReportController::progress, "/api/v1/reports/progress/{task_id}", drogon::Get);
        ADD_METHOD_TO(ReportController::previewData, "/api/v1/reports/preview-data", drogon::Get);
        ADD_METHOD_TO(ReportController::getReport, "/api/v1/reports/{id}", drogon::Get);
        ADD_METHOD_TO(ReportController::deleteReport, "/api/v1/reports/{id}", drogon::Delete);
        ADD_METHOD_TO(ReportController::getOpinion, "/api/v1/reports/{id}/opinion", drogon::Get);
        ADD_METHOD_TO(ReportController::saveOpinion, "/api/v1/reports/{id}/opinion", drogon::Put);
        ADD_METHOD_TO(ReportController::download, "/api/v1/reports/{id}/download", drogon::Get);
        ADD_METHOD_TO(ReportController::fileReport, "/api/v1/reports/{id}/file", drogon::Post);
        METHOD_LIST_END

        /**
         * @brief GET /api/v1/reports?project_id= 按项目列出报告
         * @return 通过 callback 返回报告列表
         */
        void listReports(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief POST /api/v1/reports 生成报告文档
         * @return 通过 callback 返回 201 与报告记录
         */
        void generate(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/reports/progress/{task_id} 查询报告生成进度
         * @param taskId 报告生成任务 id
         * @return 通过 callback 返回进度信息
         */
        void progress(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback, int taskId);

        /**
         * @brief GET /api/v1/reports/preview-data?project_id=&period_id= 预览报告统计数据
         * @return 通过 callback 返回报告所需的统计数据
         */
        void previewData(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback);

        /**
         * @brief GET /api/v1/reports/{id} 查询报告详情
         * @param id 报告 id
         * @return 通过 callback 返回报告记录
         */
        void getReport(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief DELETE /api/v1/reports/{id} 删除报告
         * @param id 报告 id
         * @return 通过 callback 返回 204
         */
        void deleteReport(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief GET /api/v1/reports/{id}/opinion 读取报告审核意见
         * @param id 报告 id
         * @return 通过 callback 返回审核意见
         */
        void getOpinion(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief PUT /api/v1/reports/{id}/opinion 保存报告审核意见
         * @param id 报告 id
         * @return 通过 callback 返回保存后的意见
         */
        void saveOpinion(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

        /**
         * @brief GET /api/v1/reports/{id}/download 下载报告文件
         * @param id 报告 id
         * @return 通过 callback 返回 docx 文件
         */
        void download(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                      int id);

        /**
         * @brief POST /api/v1/reports/{id}/file 标记报告已归档
         * @param id 报告 id
         * @return 通过 callback 返回归档结果; 按备案编号标记
         */
        void fileReport(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback, int id);

    private:
        std::shared_ptr<services::ReportService> reportService_;
    };

}  // namespace soilwatercm::controllers
