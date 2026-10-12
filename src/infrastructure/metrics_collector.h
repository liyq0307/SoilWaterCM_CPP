#pragma once

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <string>

namespace soilwatercm::infrastructure
{

    /**
     * @brief 进程内指标收集 (counter/gauge/histogram)
     *
     * 可导出 Prometheus 文本格式。
     */
    class MetricsCollector
    {
    public:
        /// @brief 全局唯一实例
        static MetricsCollector& instance();

        /// @brief 预留端口参数, 当前未实现
        void initialize(int port = 9090);

        /// @brief 计数器 +1
        void incrementCounter(const std::string& name, const std::map<std::string, std::string>& labels = {});

        /// @brief 计数器 -1
        void decrementCounter(const std::string& name, const std::map<std::string, std::string>& labels = {});

        /// @brief 设置计数器值
        void setCounter(const std::string& name, int64_t value, const std::map<std::string, std::string>& labels = {});

        /// @brief 向直方图记录一次观测值
        void observeHistogram(const std::string& name, double value,
                              const std::map<std::string, std::string>& labels = {});

        /// @brief 设置仪表值
        void setGauge(const std::string& name, double value, const std::map<std::string, std::string>& labels = {});

        /// @brief 仪表值增加
        void incrementGauge(const std::string& name, double value = 1.0,
                            const std::map<std::string, std::string>& labels = {});

        /// @brief 仪表值减少
        void decrementGauge(const std::string& name, double value = 1.0,
                            const std::map<std::string, std::string>& labels = {});

        /// @brief 输出 Prometheus 抓取格式的文本
        std::string exportPrometheus();

        /// @brief 记录一次 HTTP 请求: 次数 + 耗时直方图
        void recordRequest(const std::string& method, const std::string& path, int status_code, double duration_ms);

        /// @brief 记录后台任务状态 (running/completed/failed)
        void recordTask(const std::string& task_type, const std::string& status);

        /// @brief 记录一次数据库查询耗时
        void recordDatabaseQuery(const std::string& query_type, double duration_ms);

        /// @brief 清空所有指标
        void reset();

        MetricsCollector(const MetricsCollector&) = delete;
        MetricsCollector& operator=(const MetricsCollector&) = delete;
        MetricsCollector(MetricsCollector&&) = delete;
        MetricsCollector& operator=(MetricsCollector&&) = delete;

    private:
        MetricsCollector() = default;
        ~MetricsCollector() = default;

        std::string formatLabels(const std::map<std::string, std::string>& labels);
        std::string sanitizeName(const std::string& name);

        std::map<std::string, std::atomic<int64_t>> counters_;
        std::map<std::string, double> gauges_;
        std::map<std::string, std::vector<double>> histograms_;
        mutable std::mutex mutex_;

        std::atomic<int64_t> total_requests_{0};
        std::atomic<int64_t> active_requests_{0};
        std::atomic<int64_t> total_tasks_{0};
        std::atomic<int64_t> active_tasks_{0};
    };

    /**
     * @brief 作用域计时器
     *
     * 析构时把耗时记入 MetricsCollector 的直方图。
     */
    class Timer
    {
    public:
        /**
         * @brief 构造并开始计时
         * @param name 直方图指标名
         * @param labels 指标标签
         */
        explicit Timer(const std::string& name, const std::map<std::string, std::string>& labels = {});
        ~Timer();

        /// @brief 提前结束计时并上报; 析构时若未 stop 会自动调用
        void stop();

        /// @brief 至今耗时 (毫秒)
        double elapsedMs() const;

        Timer(const Timer&) = delete;
        Timer& operator=(const Timer&) = delete;
        Timer(Timer&&) = default;
        Timer& operator=(Timer&&) = default;

    private:
        std::string name_;
        std::map<std::string, std::string> labels_;
        std::chrono::steady_clock::time_point start_;
        bool stopped_ = false;
    };

}  // namespace soilwatercm::infrastructure
