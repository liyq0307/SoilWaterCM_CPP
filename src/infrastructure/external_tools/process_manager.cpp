#include "process_manager.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <memory>
#include <random>
#include <sstream>
#include <thread>

#include "../../utils/logger.h"

namespace soilwatercm::infrastructure::external_tools
{

    namespace
    {

        std::string buildCommandLine(const std::string& command, const std::vector<std::string>& args)
        {
            std::ostringstream oss;
            oss << command;
            for (const auto& arg : args)
            {
                if (arg.find(' ') != std::string::npos || arg.find('\t') != std::string::npos)
                {
                    oss << " \"" << arg << "\"";
                }
                else
                {
                    oss << " " << arg;
                }
            }
            return oss.str();
        }

        ProcessResult runPopen(const std::string& cmdline, const std::string& working_dir)
        {
            ProcessResult result;
            result.exit_code = -1;
            result.timed_out = false;

            std::string fullCmd = cmdline;
            if (!working_dir.empty())
            {
#ifdef _WIN32
                fullCmd = "cd /d \"" + working_dir + "\" && " + cmdline;
#else
                fullCmd = "cd \"" + working_dir + "\" && " + cmdline;
#endif
            }

#ifdef _WIN32
            FILE* pipe = _popen(fullCmd.c_str(), "r");
#else
            FILE* pipe = popen(fullCmd.c_str(), "r");
#endif

            if (!pipe)
            {
                result.error = "Failed to start process: " + cmdline;
                return result;
            }

            std::array<char, 4096> buffer;
            std::string output;
            while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
            {
                output += buffer.data();
            }

#ifdef _WIN32
            result.exit_code = _pclose(pipe);
#else
            result.exit_code = pclose(pipe);
#endif

            result.output = output;
            return result;
        }

    }  // anonymous namespace

    ProcessManager::ProcessManager() {}

    ProcessManager::~ProcessManager()
    {
        shutdown_ = true;
    }

    ProcessResult ProcessManager::execute(const std::string& command, const std::vector<std::string>& args,
                                          const std::string& working_dir)
    {
        return executeWithTimeout(command, args, -1, working_dir);
    }

    ProcessResult ProcessManager::executeWithTimeout(const std::string& command, const std::vector<std::string>& args,
                                                     int timeout_seconds, const std::string& working_dir)
    {
        std::string cmdline = buildCommandLine(command, args);

        if (timeout_seconds <= 0)
        {
            return runPopen(cmdline, working_dir);
        }

        ProcessResult result;
        std::thread t([&]() { result = runPopen(cmdline, working_dir); });

        if (t.joinable())
        {
            auto start = std::chrono::steady_clock::now();
            while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count() <
                   timeout_seconds)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            t.detach();
            result.exit_code = -1;
            result.timed_out = true;
            result.error = "Process timed out after " + std::to_string(timeout_seconds) + " seconds";
        }

        return result;
    }

    std::string ProcessManager::executeAsync(const std::string& command, const std::vector<std::string>& args,
                                             const std::string& working_dir)
    {
        static std::atomic<int> counter{0};
        std::string cmdline = buildCommandLine(command, args);

        std::string procId = "proc_" + std::to_string(++counter);

        std::thread(
            [this, cmdline, working_dir, procId]()
            {
                if (shutdown_)
                    return;
                runPopen(cmdline, working_dir);
            })
            .detach();

        return procId;
    }

    bool ProcessManager::cancel(const std::string& process_id)
    {
        utils::Logger::warn("Process cancellation not supported: {}", process_id);
        return false;
    }

    ProcessResult ProcessManager::wait(const std::string& process_id, int timeout_seconds)
    {
        ProcessResult result;
        result.exit_code = 0;
        result.timed_out = false;
        return result;
    }

    void ProcessManager::setEnvironment(const std::map<std::string, std::string>& env)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        environment_ = env;
        for (const auto& [key, value] : env)
        {
#ifdef _WIN32
            _putenv_s(key.c_str(), value.c_str());
#else
            setenv(key.c_str(), value.c_str(), 1);
#endif
        }
    }

    bool ProcessManager::isRunning(const std::string& process_id)
    {
        return false;
    }

    std::string ProcessManager::buildCommand(const std::string& command, const std::vector<std::string>& args)
    {
        return buildCommandLine(command, args);
    }

}  // namespace soilwatercm::infrastructure::external_tools
