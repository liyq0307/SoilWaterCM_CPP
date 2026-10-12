#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace soilwatercm::infrastructure
{

    /**
     * @brief 固定大小线程池
     *
     * 任务排队由工作线程消费, 返回 std::future 取结果。
     */
    class ThreadPool
    {
    public:
        /**
         * @brief 构造并启动工作线程
         * @param threads 线程数, 默认取硬件并发数
         */
        explicit ThreadPool(size_t threads = std::thread::hardware_concurrency());
        ~ThreadPool();

        /**
         * @brief 提交任务
         * @param f 可调用对象
         * @param args 调用参数
         * @return 可获取结果的 future; 池已停止时抛异常
         */
        template <typename F, typename... Args>
        auto submit(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type>;

        /// @brief 阻塞直到队列排空且所有在跑任务完成
        void waitAll();

        /// @brief 工作线程数
        size_t size() const { return workers_.size(); }

        /// @brief 队列中待执行任务数
        size_t pending() const;

        /// @brief 队列是否已空
        bool empty() const;

        /// @brief 暂停: 工作线程不再取新任务, resume 恢复消费
        void pause();

        /// @brief 恢复消费任务
        void resume();

        /// @brief 是否处于暂停状态
        bool isPaused() const { return paused_.load(); }

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;

    private:
        void workerThread();

        std::vector<std::thread> workers_;
        std::queue<std::function<void()>> tasks_;

        mutable std::mutex queue_mutex_;
        std::condition_variable condition_;
        std::condition_variable completion_condition_;

        std::atomic<bool> stop_{false};
        std::atomic<bool> paused_{false};
        std::atomic<size_t> active_tasks_{0};
    };

    template <typename F, typename... Args>
    auto ThreadPool::submit(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type>
    {
        using return_type = typename std::invoke_result<F, Args...>::type;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        std::future<return_type> result = task->get_future();

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_)
            {
                throw std::runtime_error("ThreadPool has been stopped");
            }

            tasks_.emplace([task]() { (*task)(); });
        }

        condition_.notify_one();
        return result;
    }

}  // namespace soilwatercm::infrastructure
