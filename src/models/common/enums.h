#pragma once

namespace soilwatercm::models
{

    /**
     * @brief 用户角色
     *
     * 库中存文本。Admin 全部权限, Monitor 可操作, Leader 只读查看。
     */
    enum class UserRole
    {
        Admin,
        Monitor,
        Leader
    };

    /**
     * @brief 项目/监测期状态
     *
     * Active 进行中, Completed 已完成, Archived 已归档。
     */
    enum class ProjectStatus
    {
        Active,
        Completed,
        Archived
    };

    /**
     * @brief 监测期类型
     *
     * 月度/季度/年度, Emergency 为突发应急监测。
     */
    enum class PeriodType
    {
        Monthly,
        Quarterly,
        Annual,
        Emergency
    };

    /**
     * @brief 影像数据类型
     *
     * Optical 光学遥感, Sar 雷达, Uav 无人机, Dem 数字高程模型。
     */
    enum class ImageType
    {
        Optical,
        Sar,
        Uav,
        Dem
    };

    /**
     * @brief 影像质检状态
     *
     * 待检/合格/不合格。Passed/Failed 是 Valid/Invalid 的别名,
     * 同一个值两种叫法, 兼容质检结果 passed/failed 的说法。
     */
    enum class QualityStatus
    {
        Pending,
        Valid,
        Invalid,
        Passed = Valid,    ///< 兼容 passed 写法
        Failed = Invalid   ///< 兼容 failed 写法
    };

    /**
     * @brief 扰动类型
     *
     * 开挖/弃渣/堆土/临时占地/其他。Construction 归为开挖、
     * Illegal 归为其他, 只是别名, 便于按业务口径书写。
     */
    enum class DisturbanceType
    {
        Excavation,
        Dump,
        Spoil,
        Temporary,
        Other,
        Construction = Excavation,  ///< 施工扰动, 按开挖记
        Illegal = Other             ///< 违规扰动, 按其他记
    };

    /**
     * @brief 水保措施大类
     *
     * 工程/植物/临时。拦渣坝、挡墙、排水沟等具体工程措施都算
     * Engineering, 绿化种草等算 Plant, 细节区分放在 measures.subtype 字段。
     */
    enum class MeasureType
    {
        Engineering,
        Plant,
        Temporary,
        CheckDam = Engineering,         ///< 拦渣坝
        RetainingWall = Engineering,    ///< 挡墙
        DrainageDitch = Engineering,    ///< 排水沟
        Greening = Plant,               ///< 绿化
        Grassing = Plant,               ///< 种草
        Afforestation = Plant,          ///< 造林
        TempCover = Temporary,          ///< 临时覆盖
        TempBarrier = Temporary,        ///< 临时拦挡
        TempDrainage = Temporary        ///< 临时排水
    };

    /**
     * @brief 措施运行状态
     *
     * 在用/停用/损毁。
     */
    enum class MeasureStatus
    {
        Active,
        Inactive,
        Damaged
    };

    /**
     * @brief 措施完好程度
     *
     * 好/一般/差。
     */
    enum class MeasureCondition
    {
        Good,
        Fair,
        Poor
    };

    /**
     * @brief 异常类型
     *
     * 违规扰动/边坡坍塌/堆土流失/InSAR 形变/侵蚀沟。
     */
    enum class AnomalyType
    {
        DisturbanceViolation,
        SlopeCollapse,
        SpoilLoss,
        InsarDeformation,
        ErosionGully
    };

    /**
     * @brief 异常严重程度
     *
     * 从轻到重四级。
     */
    enum class Severity
    {
        Low,
        Medium,
        High,
        Critical
    };

    /**
     * @brief 异常处置进度
     *
     * 待处理/已派单/处理中/已完成。Resolved 是 Completed 的别名,
     * 兼容 Python 端用 resolved 表示已处置的老写法。
     */
    enum class DisposalStatus
    {
        Pending,
        Assigned,
        Processing,
        Completed,
        Resolved = Completed  ///< 兼容 resolved 老写法
    };

    /**
     * @brief 合规评价等级
     *
     * 优秀/良好/合格/不合格。
     */
    enum class ComplianceLevel
    {
        Excellent,
        Good,
        Qualified,
        Unqualified
    };

    /**
     * @brief 风险等级
     *
     * 高/中/低。
     */
    enum class RiskLevel
    {
        High,
        Medium,
        Low
    };

    /**
     * @brief 报告类型
     *
     * 季度报告/年度报告/总结报告。
     */
    enum class ReportType
    {
        Quarterly,
        Annual,
        Summary
    };

    /**
     * @brief 处理任务类型
     *
     * 预处理/植被/侵蚀/扰动/措施/InSAR/三维/报告。
     * 后半段的长名字 (FvcExtraction 等) 是前面短名字的别名, 同一个值两种叫法。
     */
    enum class TaskType
    {
        Preprocess,
        Fvc,
        Erosion,
        Disturbance,
        Measure,
        Insar,
        ThreeD,
        Report,
        FvcExtraction = Fvc,                    ///< 植被提取
        ErosionCalculation = Erosion,            ///< 侵蚀计算
        DisturbanceExtraction = Disturbance,     ///< 扰动提取
        MeasureInterpretation = Measure,         ///< 措施解译
        Evaluation = Report                      ///< 评价归入报告类任务
    };

    /**
     * @brief 任务状态
     *
     * 排队/运行/完成/失败/超时。Cancelled 是 Failed 的别名,
     * 取消的任务在库里按失败记录。
     */
    enum class TaskStatus
    {
        Pending,
        Running,
        Completed,
        Failed,
        Timeout,
        Cancelled = Failed  ///< 取消按失败记录
    };

}  // namespace soilwatercm::models
