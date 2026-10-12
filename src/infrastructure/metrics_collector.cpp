#include "metrics_collector.h"

#include <algorithm>
#include <sstream>

namespace soilwatercm::infrastructure
{

    MetricsCollector& MetricsCollector::instance()
    {
        static MetricsCollector instance;
        return instance;
    }

    void MetricsCollector::initialize(int port) {}

    void MetricsCollector::incrementCounter(const std::string& name, const std::map<std::string, std::string>& labels)
    {
        std::string key = sanitizeName(name) + formatLabels(labels);

        std::lock_guard<std::mutex> lock(mutex_);
        if (counters_.find(key) == counters_.end())
        {
            counters_[key] = 0;
        }
        counters_[key]++;
    }

    void MetricsCollector::decrementCounter(const std::string& name, const std::map<std::string, std::string>& labels)
    {
        std::string key = sanitizeName(name) + formatLabels(labels);

        std::lock_guard<std::mutex> lock(mutex_);
        if (counters_.find(key) != counters_.end())
        {
            counters_[key]--;
        }
    }

    void MetricsCollector::setCounter(const std::string& name, int64_t value,
                                      const std::map<std::string, std::string>& labels)
    {
        std::string key = sanitizeName(name) + formatLabels(labels);

        std::lock_guard<std::mutex> lock(mutex_);
        counters_[key] = value;
    }

    void MetricsCollector::observeHistogram(const std::string& name, double value,
                                            const std::map<std::string, std::string>& labels)
    {
        std::string key = sanitizeName(name) + formatLabels(labels);

        std::lock_guard<std::mutex> lock(mutex_);
        histograms_[key].push_back(value);
    }

    void MetricsCollector::setGauge(const std::string& name, double value,
                                    const std::map<std::string, std::string>& labels)
    {
        std::string key = sanitizeName(name) + formatLabels(labels);

        std::lock_guard<std::mutex> lock(mutex_);
        gauges_[key] = value;
    }

    void MetricsCollector::incrementGauge(const std::string& name, double value,
                                          const std::map<std::string, std::string>& labels)
    {
        std::string key = sanitizeName(name) + formatLabels(labels);

        std::lock_guard<std::mutex> lock(mutex_);
        gauges_[key] += value;
    }

    void MetricsCollector::decrementGauge(const std::string& name, double value,
                                          const std::map<std::string, std::string>& labels)
    {
        std::string key = sanitizeName(name) + formatLabels(labels);

        std::lock_guard<std::mutex> lock(mutex_);
        gauges_[key] -= value;
    }

    std::string MetricsCollector::exportPrometheus()
    {
        std::ostringstream oss;

        std::lock_guard<std::mutex> lock(mutex_);

        oss << "# HELP soilwatercm_total_requests Total number of requests\n";
        oss << "# TYPE soilwatercm_total_requests counter\n";
        oss << "soilwatercm_total_requests " << total_requests_.load() << "\n\n";

        oss << "# HELP soilwatercm_active_requests Number of active requests\n";
        oss << "# TYPE soilwatercm_active_requests gauge\n";
        oss << "soilwatercm_active_requests " << active_requests_.load() << "\n\n";

        oss << "# HELP soilwatercm_total_tasks Total number of tasks\n";
        oss << "# TYPE soilwatercm_total_tasks counter\n";
        oss << "soilwatercm_total_tasks " << total_tasks_.load() << "\n\n";

        oss << "# HELP soilwatercm_active_tasks Number of active tasks\n";
        oss << "# TYPE soilwatercm_active_tasks gauge\n";
        oss << "soilwatercm_active_tasks " << active_tasks_.load() << "\n\n";

        for (const auto& [key, value] : counters_)
        {
            oss << key << " " << value.load() << "\n";
        }

        for (const auto& [key, value] : gauges_)
        {
            oss << key << " " << value << "\n";
        }

        return oss.str();
    }

    void MetricsCollector::recordRequest(const std::string& method, const std::string& path, int status_code,
                                         double duration_ms)
    {
        total_requests_++;

        std::map<std::string, std::string> labels = {
            {"method", method}, {"path", path}, {"status", std::to_string(status_code)}};

        incrementCounter("http_requests_total", labels);
        observeHistogram("http_request_duration_ms", duration_ms, labels);
    }

    void MetricsCollector::recordTask(const std::string& task_type, const std::string& status)
    {
        total_tasks_++;

        std::map<std::string, std::string> labels = {{"type", task_type}, {"status", status}};

        incrementCounter("tasks_total", labels);
    }

    void MetricsCollector::recordDatabaseQuery(const std::string& query_type, double duration_ms)
    {
        std::map<std::string, std::string> labels = {{"type", query_type}};

        incrementCounter("db_queries_total", labels);
        observeHistogram("db_query_duration_ms", duration_ms, labels);
    }

    void MetricsCollector::reset()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        counters_.clear();
        gauges_.clear();
        histograms_.clear();

        total_requests_ = 0;
        active_requests_ = 0;
        total_tasks_ = 0;
        active_tasks_ = 0;
    }

    std::string MetricsCollector::formatLabels(const std::map<std::string, std::string>& labels)
    {
        if (labels.empty())
        {
            return "";
        }

        std::ostringstream oss;
        oss << "{";

        bool first = true;
        for (const auto& [key, value] : labels)
        {
            if (!first)
            {
                oss << ",";
            }
            oss << key << "=\"" << value << "\"";
            first = false;
        }

        oss << "}";
        return oss.str();
    }

    std::string MetricsCollector::sanitizeName(const std::string& name)
    {
        std::string result = name;
        std::replace(result.begin(), result.end(), '-', '_');
        std::replace(result.begin(), result.end(), '.', '_');
        return result;
    }

    Timer::Timer(const std::string& name, const std::map<std::string, std::string>& labels)
        : name_(name), labels_(labels), start_(std::chrono::steady_clock::now())
    {
    }

    Timer::~Timer()
    {
        if (!stopped_)
        {
            stop();
        }
    }

    void Timer::stop()
    {
        if (stopped_)
        {
            return;
        }

        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_);

        MetricsCollector::instance().observeHistogram(name_, static_cast<double>(duration.count()), labels_);
        stopped_ = true;
    }

    double Timer::elapsedMs() const
    {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_);
        return static_cast<double>(duration.count());
    }

}  // namespace soilwatercm::infrastructure
