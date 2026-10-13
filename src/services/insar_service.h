#pragma once

#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../dao/task_dao.h"
#include "../infrastructure/external_tools/mintpy_interface.h"
#include "../infrastructure/external_tools/pyaps3_interface.h"
#include "../infrastructure/insar/deformation_calc.h"
#include "../infrastructure/insar/interferogram.h"
#include "../infrastructure/insar/phase_filter.h"
#include "../infrastructure/insar/snaphu_wrapper.h"
#include "../models/task.h"

namespace soilwatercm::services
{

    /** @brief InSAR 处理入参 */
    struct InsarProcessParams
    {
        int project_id;                     ///< 项目 id
        int period_id;                      ///< 期次 id
        std::string master_slc_path;        ///< 主影像 SLC 路径
        std::string slave_slc_path;         ///< 从影像 SLC 路径
        std::string output_dir;             ///< 输出目录
        std::string process_type = "dinsar"; ///< 处理流程: dinsar / sbas
    };

    /** @brief InSAR 处理结果 */
    struct InsarProcessResult
    {
        bool success = false;                                    ///< 是否成功
        std::string message;                                     ///< 结果说明/失败原因
        std::optional<std::string> interferogram_path;           ///< 干涉图路径
        std::optional<std::string> filtered_phase_path;          ///< 滤波相位路径
        std::optional<std::string> unwrapped_phase_path;         ///< 解缠相位路径
        std::optional<std::string> deformation_path;             ///< 形变图路径
        std::optional<int> task_id;                              ///< 关联任务 id
    };

    /**
     * @brief InSAR 形变监测服务
     *
     * D-InSAR 单对处理 (干涉 -> 滤波 -> 解缠 -> 形变 -> 大气校正),
     * SBAS 时序分析走 MintPy; 外部工具不可用时对应步骤跳过或报错。
     */
    class InsarService
    {
    public:
        /**
         * @brief 构造, 注入任务 DAO 与各 InSAR 处理组件
         * @param taskDao 任务 DAO (进度回写)
         * @param snaphu Snaphu 解缠封装
         * @param interferogram 干涉图生成器
         * @param phaseFilter 相位滤波器
         * @param deformationCalc 形变计算器
         * @param mintpy MintPy 接口 (SBAS 时序分析)
         * @param pyaps3 PyAPS3 接口 (大气校正)
         */
        explicit InsarService(std::shared_ptr<dao::TaskDao> taskDao,
                              std::shared_ptr<infrastructure::SnaphuWrapper> snaphu,
                              std::shared_ptr<infrastructure::InterferogramGenerator> interferogram,
                              std::shared_ptr<infrastructure::PhaseFilter> phaseFilter,
                              std::shared_ptr<infrastructure::DeformationCalculator> deformationCalc,
                              std::shared_ptr<infrastructure::MintPyInterface> mintpy,
                              std::shared_ptr<infrastructure::Pyaps3Interface> pyaps3);

        ~InsarService() = default;

        /**
         * @brief 异步提交 InSAR 任务
         * @param params 处理入参
         * @return 结果 future; 按 process_type 走 D-InSAR 或 SBAS 流程
         */
        std::future<InsarProcessResult> submitInsarTask(const InsarProcessParams& params);

        /**
         * @brief 复用已有任务记录执行 InSAR (控制器先建任务再执行时走此重载)
         * @param params 处理入参
         * @param taskId 已登记的任务 id
         * @return 结果 future
         */
        std::future<InsarProcessResult> submitInsarTask(const InsarProcessParams& params, int taskId);

        /**
         * @brief D-InSAR 主流程
         *
         * 干涉图 -> Goldstein 滤波 -> Snaphu 解缠 -> LOS 形变 -> 大气校正;
         * 任一步异常以失败结果返回并回写任务。
         * @param params 处理入参
         * @param taskId 关联任务 id
         * @return 处理结果 (含各中间产物路径)
         */
        InsarProcessResult executeDInsar(const InsarProcessParams& params, int taskId);

        /**
         * @brief SBAS 时序分析
         * @param params 处理入参
         * @param taskId 关联任务 id
         * @return 处理结果; MintPy 不可用则失败
         */
        InsarProcessResult executeSbas(const InsarProcessParams& params, int taskId);

        /**
         * @brief 主从 SLC 配准相干生成干涉图
         * @param masterPath 主影像 SLC 路径
         * @param slavePath 从影像 SLC 路径
         * @param outputPath 输出路径
         * @return 输出路径
         */
        std::string generateInterferogram(const std::string& masterPath, const std::string& slavePath,
                                          const std::string& outputPath);

        /**
         * @brief 相位滤波去噪
         * @param ifgPath 干涉图路径
         * @param outputPath 输出路径
         * @param method 滤波方法 (goldstein 等)
         * @return 输出路径
         */
        std::string filterPhase(const std::string& ifgPath, const std::string& outputPath,
                                const std::string& method = "goldstein");

        /**
         * @brief Snaphu 相位解缠
         * @param phasePath 滤波相位路径
         * @param coherencePath 相干文件路径
         * @param outputPath 输出路径
         * @return 输出路径
         */
        std::string unwrapPhase(const std::string& phasePath, const std::string& coherencePath,
                                const std::string& outputPath);

        /**
         * @brief 解缠相位换算 LOS 形变
         *
         * 形变量 = -相位 * 波长 / 4π (d = -λφ/4π)。
         * @param unwrappedPath 解缠相位路径
         * @param outputPath 输出路径
         * @param wavelength 雷达波长 (米), 默认 C 波段 0.056
         * @return 输出路径
         */
        std::string calculateDeformation(const std::string& unwrappedPath, const std::string& outputPath,
                                         double wavelength = 0.056);

        /**
         * @brief 大气校正
         * @param deformationPath 形变图路径
         * @param outputPath 输出路径
         * @return 校正后路径; PyAPS3 不可用则原样返回输入路径
         */
        std::string atmosphericCorrection(const std::string& deformationPath, const std::string& outputPath);

        /// @brief Snaphu 外部工具可用性检查
        bool checkSnaphuAvailable();

        /// @brief MintPy 外部工具可用性检查
        bool checkMintpyAvailable();

    private:
        std::shared_ptr<dao::TaskDao> taskDao_;
        std::shared_ptr<infrastructure::SnaphuWrapper> snaphu_;
        std::shared_ptr<infrastructure::InterferogramGenerator> interferogram_;
        std::shared_ptr<infrastructure::PhaseFilter> phaseFilter_;
        std::shared_ptr<infrastructure::DeformationCalculator> deformationCalc_;
        std::shared_ptr<infrastructure::MintPyInterface> mintpy_;
        std::shared_ptr<infrastructure::Pyaps3Interface> pyaps3_;

        void updateTaskProgress(int taskId, int progress, const std::string& message);
    };

}  // namespace soilwatercm::services
