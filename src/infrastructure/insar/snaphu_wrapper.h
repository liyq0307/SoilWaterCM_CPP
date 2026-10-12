#pragma once

#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "../gdal/gdal_wrapper.h"

namespace soilwatercm::infrastructure
{

    using soilwatercm::infrastructure::gdal::GdalWrapper;
    using soilwatercm::infrastructure::gdal::RasterData;

    /// SNAPHU 解缠参数 (对应 snaphu 配置文件各项)
    struct SnaphuParams
    {
        int costMode = 0;
        int initMethod = 0;
        double initOnly = 0.0;
        int nLook = 1;
        double maxComponent = 0.0;
        double costScale = 1.0;
        double p1 = 0.5;
        double p2 = 0.5;
        double p3 = 0.5;
        double p4 = 0.5;
        double p5 = 0.5;
        double p6 = 0.5;
        double p7 = 0.5;
        double p8 = 0.5;
        double p9 = 0.5;
        int verbose = 1;
        int rowColConv = 0;
        int tileSizeRow = 0;
        int tileSizeCol = 0;
        int nTileRow = 0;
        int nTileCol = 0;
        double maxConnCompCost = 0.0;
        double threshold = 0.0;
        double maxFlow = 0.0;
        int algorithm = 0;
    };

    /// 解缠结果; rewrappedPhase 当前流程未填充, 恒为空
    struct SnaphuResult
    {
        bool success;
        std::string errorMessage;
        RasterData unwrappedPhase;
        std::optional<RasterData> rewrappedPhase;
    };

    /**
     * @brief SNAPHU 相位解缠封装
     *
     * 把缠绕相位写成临时二进制文件, 生成配置文件后调用 snaphu 求解。
     */
    class SnaphuWrapper
    {
    public:
        SnaphuWrapper();

        ~SnaphuWrapper();

        /**
         * @brief 内存数据解缠
         * @param wrappedPhase 缠绕相位
         * @param coherence 相干性图 (用作代价权重)
         * @param params 解缠参数
         * @return 解缠结果; SNAPHU 不可用时返回失败
         */
        SnaphuResult unwrap(const RasterData& wrappedPhase, const RasterData& coherence,
                            const SnaphuParams& params = SnaphuParams{});

        /**
         * @brief 直接读影像文件解缠
         * @param wrappedPhaseFile 缠绕相位文件
         * @param coherenceFile 相干性文件
         * @param outputFile 结果输出文件
         * @param params 解缠参数
         * @return 解缠结果
         */
        SnaphuResult unwrapFile(const std::string& wrappedPhaseFile, const std::string& coherenceFile,
                                const std::string& outputFile, const SnaphuParams& params = SnaphuParams{});

        /// @brief 异步版 unwrap
        std::future<SnaphuResult> unwrapAsync(const RasterData& wrappedPhase, const RasterData& coherence,
                                              const SnaphuParams& params = SnaphuParams{});

        /// @brief SNAPHU 是否可用
        bool isAvailable() const { return available_; }

        /// @brief SNAPHU 版本
        std::string getVersion() const { return version_; }

        /// @brief 把参数转成 SNAPHU 配置文件文本
        static std::string paramsToConfig(const SnaphuParams& params);

    private:
        class SnaphuState;
        std::unique_ptr<SnaphuState> state_;
        std::shared_ptr<spdlog::logger> logger_;
        bool available_ = false;
        std::string version_;

        bool checkSnaphuAvailable();

        bool writeConfigFile(const std::string& configPath, const SnaphuParams& params, int width, int height);

        SnaphuResult executeSnaphu(const std::string& wrappedFile, const std::string& coherenceFile,
                                   const std::string& outputFile, const std::string& configFile);

        RasterData readUnwrappedPhase(const std::string& filename);
    };

}  // namespace soilwatercm::infrastructure
