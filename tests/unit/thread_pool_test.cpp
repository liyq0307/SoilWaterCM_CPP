#include "test_base.h"
#include "../../src/infrastructure/thread_pool.h"
#include <atomic>
#include <chrono>

namespace soilwatercm::tests {

class ThreadPoolTest : public InfrastructureTestBase {
protected:
    void SetUp() override {
        InfrastructureTestBase::SetUp();
        threadPool = std::make_unique<infrastructure::ThreadPool>(4);
    }
    
    void TearDown() override {
        threadPool.reset();
        InfrastructureTestBase::TearDown();
    }
    
    std::unique_ptr<infrastructure::ThreadPool> threadPool;
};

TEST_F(ThreadPoolTest, Submit_ShouldExecuteTask) {
    std::atomic<int> counter{0};
    
    auto future = threadPool->submit([&counter]() {
        counter++;
        return 42;
    });
    
    int result = future.get();
    
    EXPECT_EQ(result, 42);
    EXPECT_EQ(counter.load(), 1);
}

TEST_F(ThreadPoolTest, Submit_ShouldHandleMultipleTasks) {
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < 10; ++i) {
        futures.push_back(threadPool->submit([&counter]() {
            counter++;
        }));
    }
    
    for (auto& future : futures) {
        future.get();
    }
    
    EXPECT_EQ(counter.load(), 10);
}

TEST_F(ThreadPoolTest, Submit_ShouldHandleExceptionInTask) {
    auto future = threadPool->submit([]() -> int {
        throw std::runtime_error("Test exception");
    });
    
    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST_F(ThreadPoolTest, Submit_ShouldReturnCorrectValue) {
    auto future = threadPool->submit([]() {
        return std::string("test_result");
    });
    
    std::string result = future.get();
    
    EXPECT_EQ(result, "test_result");
}

TEST_F(ThreadPoolTest, ConcurrentExecution_ShouldImprovePerformance) {
    auto startTime = std::chrono::steady_clock::now();
    
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 8; ++i) {
        futures.push_back(threadPool->submit([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }));
    }
    
    for (auto& future : futures) {
        future.get();
    }
    
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    EXPECT_LT(duration.count(), 500);
}

} // namespace soilwatercm::tests
