#include "thread_pool.h"

namespace soilwatercm::infrastructure
{

    ThreadPool::ThreadPool(size_t threads)
    {
        if (threads == 0)
        {
            threads = std::thread::hardware_concurrency();
            if (threads == 0)
            {
                threads = 4;
            }
        }

        for (size_t i = 0; i < threads; ++i)
        {
            workers_.emplace_back(&ThreadPool::workerThread, this);
        }
    }

    ThreadPool::~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }

        condition_.notify_all();

        for (std::thread& worker : workers_)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
    }

    void ThreadPool::workerThread()
    {
        while (true)
        {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(queue_mutex_);

                condition_.wait(lock, [this]() { return stop_ || !tasks_.empty(); });

                if (stop_ && tasks_.empty())
                {
                    return;
                }

                if (paused_)
                {
                    continue;
                }

                if (!tasks_.empty())
                {
                    task = std::move(tasks_.front());
                    tasks_.pop();
                    ++active_tasks_;
                }
            }

            if (task)
            {
                task();
                --active_tasks_;
                completion_condition_.notify_all();
            }
        }
    }

    void ThreadPool::waitAll()
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        completion_condition_.wait(lock, [this]() { return tasks_.empty() && active_tasks_ == 0; });
    }

    size_t ThreadPool::pending() const
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return tasks_.size();
    }

    bool ThreadPool::empty() const
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return tasks_.empty();
    }

    void ThreadPool::pause()
    {
        paused_ = true;
    }

    void ThreadPool::resume()
    {
        paused_ = false;
        condition_.notify_all();
    }

}  // namespace soilwatercm::infrastructure
