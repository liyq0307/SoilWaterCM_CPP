#pragma once

#include <memory>

#include <spdlog/spdlog.h>

#include "external_tool_interface.h"
#include "process_manager.h"

using namespace soilwatercm::infrastructure::external_tools;

namespace soilwatercm::infrastructure
{

    /// 可用的 InSAR 处理工具类型
    enum class InsarToolType
    {
        ISCE2,
        SNAP,
        NONE
    };

    /**
     * @brief InSAR 处理工具门面
     *
     * 构造时自动选用 ISCE2 或 SNAP (首选不可用则回退另一个), 对外提供统一算法入口。
     */
    class InsarInterface : public ExternalToolInterface
    {
    public:
        /**
         * @brief 构造并选定后端工具
         * @param toolPath 工具路径, 可空
         * @param preferredTool 首选工具; NONE 表示自动探测
         */
        explicit InsarInterface(const std::string& toolPath = "", InsarToolType preferredTool = InsarToolType::NONE);

        ~InsarInterface() override = default;

        /**
         * @brief 按当前选定工具执行算法; 工具不可用直接返回失败
         * @param algorithm 算法 id
         * @param params 参数键值表
         * @param callback 进度回调, 可空
         * @return 执行结果
         */
        ToolResult execute(const std::string& algorithm, const std::map<std::string, std::string>& params,
                           const ProgressCallback& callback = nullptr) override;

        /// @brief 异步执行, 后台线程调 execute
        std::future<ToolResult> executeAsync(const std::string& algorithm,
                                             const std::map<std::string, std::string>& params,
                                             const ProgressCallback& callback = nullptr) override;

        /**
         * @brief 两轨 D-InSAR 处理, 结果写到 outputDir
         * @param masterImage 主影像路径
         * @param slaveImage 辅影像路径
         * @param demPath DEM 路径
         * @param outputDir 输出目录
         * @return 执行结果
         */
        ToolResult performDInSAR(const std::string& masterImage, const std::string& slaveImage,
                                 const std::string& demPath, const std::string& outputDir);

        /// @brief 主辅影像配准 (转发 execute)
        ToolResult performCoregistration(const std::string& masterImage, const std::string& slaveImage,
                                         const std::string& outputDir);

        /// @brief 生成干涉图 (转发 execute)
        ToolResult generateInterferogram(const std::string& masterImage, const std::string& slaveImage,
                                         const std::string& outputDir);

        /// @brief 相位解缠 (转发 execute)
        ToolResult unwrapPhase(const std::string& wrappedPhase, const std::string& coherence,
                               const std::string& output);

        /// @brief 当前选定的后端工具
        InsarToolType getActiveTool() const { return active_tool_; }

        std::vector<AlgorithmInfo> listAlgorithms() override;

        bool isAvailable() override;

        std::string getVersion() override;

        std::string getName() override;

    private:
        std::string tool_path_;
        InsarToolType active_tool_;
        std::unique_ptr<ProcessManager> process_manager_;
        std::shared_ptr<spdlog::logger> logger_;
        bool available_ = false;
        std::string version_;

        ToolResult executeIsce2(const std::string& algorithm, const std::map<std::string, std::string>& params,
                                const ProgressCallback& callback);

        ToolResult executeSnap(const std::string& algorithm, const std::map<std::string, std::string>& params,
                               const ProgressCallback& callback);

        InsarToolType detectAvailableTool();

        std::string buildIsce2Command(const std::string& algorithm, const std::map<std::string, std::string>& params);

        std::string buildSnapCommand(const std::string& algorithm, const std::map<std::string, std::string>& params);

        bool checkIsce2Available();
        bool checkSnapAvailable();
    };

}  // namespace soilwatercm::infrastructure
