#pragma once

#include <memory>

#include <spdlog/spdlog.h>

#include "../external/qgis_client.h"
#include "external_tool_interface.h"

using namespace soilwatercm::infrastructure::external_tools;
using namespace soilwatercm::infrastructure::external;

namespace soilwatercm::infrastructure
{

    /**
     * @brief SAGA GIS 封装
     *
     * 不直接调 SAGA 命令行, 而是经 QGIS Processing 插件跑 SAGA 算法。
     * 当前构建没有 QGIS processing 后端, execute 恒返回失败。
     */
    class SagaInterface : public ExternalToolInterface
    {
    public:
        /**
         * @brief 构造, 注入 QGIS 客户端
         * @param qgisClient QGIS 客户端; 为空则本接口不可用
         */
        explicit SagaInterface(std::shared_ptr<QgisClient> qgisClient);

        ~SagaInterface() override = default;

        /**
         * @brief 执行算法 (当前未接通 QGIS processing 后端, 恒返回失败)
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

        // 以下便捷方法都是 execute 的特定算法包装

        /// @brief 地形形态学分析 (坡向/地形部位等分类)
        ToolResult analyzeTerrainMorphology(const std::string& demPath, const std::string& output);

        /// @brief 土壤可蚀性 K 因子
        ToolResult calculateKFactor(const std::string& soilDataPath, const std::string& output);

        /// @brief 地形湿度指数 TWI
        ToolResult calculateTWI(const std::string& demPath, const std::string& output);

        /// @brief 地形汇流汇聚指数
        ToolResult calculateConvergenceIndex(const std::string& demPath, const std::string& output);

        /// @brief USLE 模型的地形坡长坡度因子
        ToolResult calculateLSFactor(const std::string& demPath, const std::string& output);

        std::vector<AlgorithmInfo> listAlgorithms() override;

        bool isAvailable() override;

        std::string getVersion() override;

        std::string getName() override { return "SAGA GIS"; }

    private:
        std::shared_ptr<QgisClient> qgis_client_;
        std::shared_ptr<spdlog::logger> logger_;
        bool available_ = false;
        std::string version_;

        std::string buildProcessingParams(const std::map<std::string, std::string>& params);

        std::string extractOutputFromResult(const std::string& result);
    };

}  // namespace soilwatercm::infrastructure
