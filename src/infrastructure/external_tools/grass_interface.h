#pragma once

#include <map>
#include <memory>
#include <vector>

#include "external_tool_interface.h"
#include "process_manager.h"

namespace soilwatercm::infrastructure::external_tools
{

    /**
     * @brief GRASS GIS 命令行封装
     *
     * 调用 r.slope.aspect、r.watershed 等模块做地形分析。
     */
    class GrassInterface : public ExternalToolInterface
    {
    public:
        GrassInterface();

        /**
         * @brief 构造, 指定 grass 可执行文件路径
         * @param grass_path grass 命令路径
         */
        explicit GrassInterface(const std::string& grass_path);
        ~GrassInterface() override = default;

        bool isAvailable() override;
        std::string getName() override { return "GRASS GIS"; }
        std::string getVersion() override;

        /**
         * @brief 按算法名 + 参数键值执行 GRASS 模块
         * @param algorithm 模块名
         * @param params 参数键值表
         * @param callback 进度回调, 可空
         * @return 执行结果
         */
        ToolResult execute(const std::string& algorithm, const std::map<std::string, std::string>& params,
                           const ProgressCallback& callback = nullptr) override;

        std::future<ToolResult> executeAsync(const std::string& algorithm,
                                             const std::map<std::string, std::string>& params,
                                             const ProgressCallback& callback = nullptr) override;

        std::vector<AlgorithmInfo> listAlgorithms() override;

        /**
         * @brief 坡度 + 坡向一起算 (r.slope.aspect)
         * @param dem_path 输入 DEM
         * @param output_slope 坡度输出
         * @param output_aspect 坡向输出
         * @return 成功返回 true
         */
        bool runSlopeAspect(const std::string& dem_path, const std::string& output_slope,
                            const std::string& output_aspect);

        /// @brief 流域分割 (r.watershed, 输出 accumulation/drainage/basin/stream 四图)
        bool runWatershed(const std::string& dem_path, const std::string& output_dir);

        /// @brief 形态学运算 (r.morphology, operation 取 opening/closing 等)
        bool runMorphology(const std::string& input_path, const std::string& output_path, const std::string& operation);

        /// @brief USLE 坡长坡度因子: 先算坡度坡向再组合计算
        bool calculateLSFactor(const std::string& demPath, const std::string& outputPath);

        /// @brief 形态学运算 (同 runMorphology)
        bool morphologyProcess(const std::string& inputPath, const std::string& outputPath,
                               const std::string& operation);

        /**
         * @brief 直接传 GRASS 模块参数执行
         *
         * 结果存到 last_output_/last_error_ 供 getOutput()/getError() 取回。
         * @param args 模块参数列表
         * @param working_dir 工作目录
         * @return 退出码为 0 且未超时返回 true
         */
        bool execute(const std::vector<std::string>& args, const std::string& working_dir = "");

        /// @brief 异步执行模块参数, 不等待结果
        bool executeAsync(const std::vector<std::string>& args, const std::string& working_dir = "");

        /// @brief 上次执行的 stdout
        std::string getOutput() const;

        /// @brief 上次执行的 stderr
        std::string getError() const;

        /// @brief 上次执行的退出码
        int getExitCode() const;

        /// @brief 设置执行超时秒数 (默认 3600)
        void setTimeout(int seconds);

        /// @brief 设置子进程环境变量
        void setEnvironment(const std::map<std::string, std::string>& env);

        /// @brief 工具安装信息
        ToolInfo getInfo() const;

        /// @brief 版本检查 (当前只验证 grass --version 可执行, 不比较版本号)
        bool checkVersion(const std::string& min_version) const;

    private:
        std::string grass_path_;
        std::unique_ptr<ProcessManager> processMgr_;
        std::string last_output_;
        std::string last_error_;
        int last_exit_code_ = 0;
        int timeout_seconds_ = 3600;
    };

}  // namespace soilwatercm::infrastructure::external_tools
