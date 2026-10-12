#include "test_base.h"
#include "../../src/infrastructure/thread_pool.h"
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace soilwatercm::tests {

class ConcurrencyTest : public InfrastructureTestBase {
protected:
    void SetUp() override {
        InfrastructureTestBase::SetUp();
    }
    
    void TearDown() override {
        InfrastructureTestBase::TearDown();
    }
};

TEST_F(ConcurrencyTest, ThreadPool_ShouldHandleConcurrentTasks) {
    auto threadPool = std::make_unique<infrastructure::ThreadPool>(4);
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < 100; ++i) {
        futures.push_back(threadPool->submit([&counter]() {
            counter.fetch_add(1);
        }));
    }
    
    for (auto& future : futures) {
        future.get();
    }
    
    EXPECT_EQ(counter.load(), 100);
}

TEST_F(ConcurrencyTest, ThreadPool_ShouldPreserveOrder) {
    auto threadPool = std::make_unique<infrastructure::ThreadPool>(1);
    std::vector<int> results;
    std::mutex mutex;
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < 10; ++i) {
        futures.push_back(threadPool->submit([&results, &mutex, i]() {
            std::lock_guard<std::mutex> lock(mutex);
            results.push_back(i);
        }));
    }
    
    for (auto& future : futures) {
        future.get();
    }
    
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(results[i], i);
    }
}

TEST_F(ConcurrencyTest, AtomicOperations_ShouldBeThreadSafe) {
    std::atomic<int> counter{0};
    std::vector<std::thread> threads;
    
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&counter]() {
            for (int j = 0; j < 1000; ++j) {
                counter.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(counter.load(), 10000);
}

TEST_F(ConcurrencyTest, Mutex_ShouldProtectSharedResource) {
    std::vector<int> sharedVector;
    std::mutex mutex;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&sharedVector, &mutex, i]() {
            std::lock_guard<std::mutex> lock(mutex);
            sharedVector.push_back(i);
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(sharedVector.size(), 10);
}

TEST_F(ConcurrencyTest, ConditionVariable_ShouldSynchronizeThreads) {
    std::mutex mutex;
    std::condition_variable cv;
    bool ready = false;
    std::atomic<int> counter{0};
    
    std::thread producer([&]() {
        std::lock_guard<std::mutex> lock(mutex);
        ready = true;
        cv.notify_one();
    });
    
    std::thread consumer([&]() {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&]() { return ready; });
        counter = 1;
    });
    
    producer.join();
    consumer.join();
    
    EXPECT_EQ(counter.load(), 1);
}

TEST_F(ConcurrencyTest, FuturePromise_ShouldCommunicateResults) {
    std::promise<int> promise;
    std::future<int> future = promise.get_future();
    
    std::thread thread([&promise]() {
        promise.set_value(42);
    });
    
    int result = future.get();
    thread.join();
    
    EXPECT_EQ(result, 42);
}

TEST_F(ConcurrencyTest, Async_ShouldExecuteAsynchronously) {
    auto future = std::async(std::launch::async, []() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return 123;
    });
    
    EXPECT_EQ(future.get(), 123);
}

} // namespace soilwatercm::tests
