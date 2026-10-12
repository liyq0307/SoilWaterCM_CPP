#include "odm_client.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

#include "../../utils/logger.h"
#include "../external_tools/process_manager.h"

namespace soilwatercm::infrastructure::external
{

    OdmClient::OdmClient() : odm_path_("odm") {}

    OdmClient::OdmClient(const std::string& odm_path) : odm_path_(odm_path) {}

    void OdmClient::setOdmPath(const std::string& path)
    {
        odm_path_ = path;
    }

    std::string OdmClient::submitTask(const std::vector<std::string>& image_paths, const std::string& output_dir)
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 999999);

        std::ostringstream oss;
        oss << "odm_task_" << dis(gen);
        std::string task_id = oss.str();

        utils::Logger::info("ODM: submitting task {} with {} images", task_id, image_paths.size());

        std::vector<std::string> args;
        args.push_back("--project-path");
        args.push_back(output_dir);
        args.push_back("--name");
        args.push_back(task_id);
        for (const auto& path : image_paths)
        {
            args.push_back(path);
        }

        external_tools::ProcessManager pm;
        auto result = pm.executeAsync(odm_path_, args, output_dir);
        active_processes_[task_id] = result;

        return task_id;
    }

    OdmTask OdmClient::getTaskStatus(const std::string& task_id)
    {
        OdmTask task;
        task.id = task_id;

        auto it = active_processes_.find(task_id);
        if (it == active_processes_.end())
        {
            task.status = "not_found";
            task.progress = 0.0;
            return task;
        }

        external_tools::ProcessManager pm;
        if (pm.isRunning(it->second))
        {
            task.status = "running";
            task.progress = 50.0;
        }
        else
        {
            auto result = pm.wait(it->second, 0);
            if (result.exit_code == 0)
            {
                task.status = "completed";
                task.progress = 100.0;
                task.output_path = odm_path_ + "/" + task_id;
            }
            else
            {
                task.status = "failed";
                task.error_message = result.error;
            }
        }

        return task;
    }

    bool OdmClient::cancelTask(const std::string& task_id)
    {
        auto it = active_processes_.find(task_id);
        if (it == active_processes_.end())
            return false;

        external_tools::ProcessManager pm;
        bool cancelled = pm.cancel(it->second);
        active_processes_.erase(it);
        return cancelled;
    }

    std::string OdmClient::getResult(const std::string& task_id, const std::string& result_type)
    {
        std::string output_dir = odm_path_ + "/" + task_id;

        if (result_type == "orthophoto")
        {
            std::string path = output_dir + "/odm_orthophoto/odm_orthophoto.tif";
            if (std::filesystem::exists(path))
                return path;
        }
        else if (result_type == "dsm")
        {
            std::string path = output_dir + "/odm_dem/dsm.tif";
            if (std::filesystem::exists(path))
                return path;
        }
        else if (result_type == "dtm")
        {
            std::string path = output_dir + "/odm_dem/dtm.tif";
            if (std::filesystem::exists(path))
                return path;
        }

        return "";
    }

    bool OdmClient::isHealthy()
    {
        external_tools::ProcessManager pm;
        auto result = pm.execute(odm_path_, {"--help"});
        return result.exit_code == 0;
    }

    bool OdmClient::isAvailable()
    {
        std::filesystem::path odmPath(odm_path_);
        if (std::filesystem::exists(odmPath))
        {
            return true;
        }

        external_tools::ProcessManager pm;
        auto result = pm.execute(odm_path_, {"--version"});
        return result.exit_code == 0;
    }

    bool OdmClient::convertTo3DTiles(const std::string& inputPath, const std::string& outputPath)
    {
        utils::Logger::info("ODM: converting to 3D Tiles: {} -> {}", inputPath, outputPath);

        std::filesystem::create_directories(outputPath);

        std::string tilesetPath = outputPath + "/tileset.json";

        std::ofstream ofs(tilesetPath);
        if (!ofs.is_open())
        {
            utils::Logger::error("Failed to create tileset.json: {}", tilesetPath);
            return false;
        }

        ofs << R"({
  "asset": {
    "version": "1.0",
    "tilesetVersion": "1.0.0"
  },
  "geometricError": 500.0,
  "root": {
    "boundingVolume": {
      "box": [0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1]
    },
    "geometricError": 0,
    "content": {
      "uri": "model.obj"
    }
  }
})";
        ofs.close();

        return true;
    }

}  // namespace soilwatercm::infrastructure::external
