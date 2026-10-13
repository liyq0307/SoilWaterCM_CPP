#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../dao/anomaly_dao.h"
#include "../dao/erosion_result_dao.h"
#include "../dao/disturbance_dao.h"
#include "../dao/terrain_result_dao.h"
#include "../models/anomaly.h"
#include "../models/disturbance.h"
#include "../utils/logger.h"

namespace soilwatercm::services
{

    /** @brief 异常检测入参 */
    struct AnomalyDetectParams
    {
        int project_id;                             ///< 项目 id
        int period_id;                              ///< 期次 id
        std::string type;                           ///< 异常类型 (disturbance/deformation)
        std::optional<models::Geometry> area;       ///< 检测范围; 为空则全项目
    };

    /**
     * @brief 异常检测与处置服务
     *
     * 从扰动分类和 InSAR 形变结果生成异常记录,
     * 并管理预警、派单、分配、更新、销号等处置流程。
     */
    class AnomalyService
    {
    public:
        /**
         * @brief 构造, 注入异常 DAO 与 (可选) 各检测源 DAO
         * @param anomalyDao 异常 DAO
         * @param disturbanceDao 扰动 DAO (违规扰动检测数据源), 可为空
         * @param terrainDao 地形成果 DAO (InSAR 形变栅格来源), 可为空
         * @param erosionDao 侵蚀成果 DAO (剧烈侵蚀检测数据源), 可为空
         */
        AnomalyService(std::shared_ptr<dao::AnomalyDao> anomalyDao,
                       std::shared_ptr<dao::DisturbanceDao> disturbanceDao = nullptr,
                       std::shared_ptr<dao::TerrainResultDao> terrainDao = nullptr,
                       std::shared_ptr<dao::ErosionResultDao> erosionDao = nullptr);

        ~AnomalyService() = default;

        /**
         * @brief 按类型检测异常并入库
         *
         * disturbance: 取该期次违规 (Illegal) 扰动图斑生成异常;
         * deformation:  取该项目最新 InSAR 形变栅格做形变异常检测;
         * erosion:      取该期次侵蚀成果, 剧烈/极强烈面积超阈值则生成一条异常。
         * 对应数据源 DAO 未注入时记日志并返回空列表 (不阻塞其他类型)。
         * @param params 检测入参
         * @return 本次新检测生成 (已入库) 的异常列表
         */
        std::vector<models::Anomaly> detectAnomalies(const AnomalyDetectParams& params);

        /**
         * @brief 扰动违规检测
         * @param projectId 项目 id
         * @param periodId 期次 id
         * @param disturbances 扰动图斑列表
         * @return 生成的异常列表 (挑出 Illegal 类型图斑, 高严重度入库)
         */
        std::vector<models::Anomaly> detectDisturbanceViolation(int projectId, int periodId,
                                                                const std::vector<models::Disturbance>& disturbances);

        /**
         * @brief InSAR 形变异常检测
         *
         * 读形变栅格, |形变|>threshold 的像元做邻域连通标记,
         * 每块 >=5 像元的区域生成一条异常 (外接矩形范围), 严重度按最大形变分高/中/低。
         * @param projectId 项目 id
         * @param periodId 期次 id
         * @param deformationPath 形变栅格路径
         * @param threshold 形变阈值
         * @return 生成的异常列表; 栅格读取失败时记日志返回空列表
         */
        std::vector<models::Anomaly> detectInsarDeformation(int projectId, int periodId,
                                                            const std::string& deformationPath, double threshold = 0.1);

        /**
         * @brief 推送预警
         * @param anomaly 异常记录
         * @return 当前为占位实现, 仅记日志并返回 true
         */
        bool sendAlert(const models::Anomaly& anomaly);

        /**
         * @brief 发送邮件
         * @param to 收件人
         * @param subject 主题
         * @param body 正文
         * @return 当前为占位实现, 仅记日志并返回 true
         */
        bool sendEmail(const std::string& to, const std::string& subject, const std::string& body);

        /**
         * @brief 发送短信
         * @param phone 手机号
         * @param message 短信内容
         * @return 当前为占位实现, 仅记日志并返回 true
         */
        bool sendSms(const std::string& phone, const std::string& message);

        /**
         * @brief 指定处置人
         * @param anomalyId 异常主键
         * @param assigneeId 处置人 id
         * @return 更新成功返回 true
         */
        bool assignAnomaly(int anomalyId, int assigneeId);

        /**
         * @brief 派单 (无人机详查)
         * @param anomalyId 异常主键
         * @return 置为已分配状态; 异常不存在或已完成返回 false
         */
        bool dispatchAnomaly(int anomalyId);

        /**
         * @brief 按通道推送预警
         * @param anomalyId 异常主键
         * @return 走 sendAlert 占位实现; 异常不存在返回 false
         */
        bool notifyAnomaly(int anomalyId);

        /**
         * @brief 按需更新异常字段
         * @param anomalyId 异常主键
         * @param status 处置状态; nullopt 不改, 置为已完成时补记解决时间
         * @param severity 严重度
         * @param description 描述
         * @param measures 处置措施
         * @param assignee 处置人
         * @return 更新成功返回 true; 异常不存在返回 false
         */
        bool updateAnomaly(int anomalyId, const std::optional<models::DisposalStatus>& status,
                           const std::optional<models::Severity>& severity,
                           const std::optional<std::string>& description, const std::optional<std::string>& measures,
                           const std::optional<std::string>& assignee);

        /**
         * @brief 销号
         * @param anomalyId 异常主键
         * @param measures 处置措施
         * @return 置为已解决并记录解决时间; 异常不存在返回 false
         */
        bool resolveAnomaly(int anomalyId, const std::string& measures);

        /**
         * @brief 按项目查异常列表
         * @param projectId 项目 id
         * @return 异常列表
         */
        std::vector<models::Anomaly> getAnomaliesByProject(int projectId);

        /**
         * @brief 按处置状态查异常列表
         * @param status 处置状态
         * @return 异常列表
         */
        std::vector<models::Anomaly> getAnomaliesByStatus(models::DisposalStatus status);

        /**
         * @brief 按 id 查单条异常
         * @param id 异常主键
         * @return 记录; 不存在返回 nullopt
         */
        std::optional<models::Anomaly> getAnomalyById(int id);

    private:
        std::shared_ptr<dao::AnomalyDao> anomalyDao_;
        std::shared_ptr<dao::DisturbanceDao> disturbanceDao_;
        std::shared_ptr<dao::TerrainResultDao> terrainDao_;
        std::shared_ptr<dao::ErosionResultDao> erosionDao_;
    };

}  // namespace soilwatercm::services
