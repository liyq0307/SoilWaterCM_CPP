#include "insar_client.h"

#include <random>
#include <sstream>

namespace soilwatercm::infrastructure::external
{

    InsarClient::InsarClient() : current_tool_("isce2") {}

    std::string InsarClient::submitDinsarTask(const std::string& master_image, const std::string& slave_image,
                                              const std::string& output_dir)
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 999999);

        std::ostringstream oss;
        oss << "insar_task_" << dis(gen);
        return oss.str();
    }

    std::string InsarClient::submitTimeseriesTask(const std::vector<std::string>& images, const std::string& output_dir)
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 999999);

        std::ostringstream oss;
        oss << "insar_ts_task_" << dis(gen);
        return oss.str();
    }

    InsarTask InsarClient::getTaskStatus(const std::string& task_id)
    {
        InsarTask task;
        task.id = task_id;
        task.status = "completed";
        task.progress = 100.0;
        return task;
    }

    bool InsarClient::cancelTask(const std::string& task_id)
    {
        return true;
    }

    std::string InsarClient::getResult(const std::string& task_id, const std::string& result_type)
    {
        return "";
    }

    void InsarClient::setTool(const std::string& tool_name)
    {
        current_tool_ = tool_name;
    }

    bool InsarClient::isHealthy()
    {
        return true;
    }

}  // namespace soilwatercm::infrastructure::external
