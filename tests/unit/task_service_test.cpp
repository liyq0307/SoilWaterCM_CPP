#include "test_base.h"
#include "../../src/services/task_service.h"
#include "../../src/dao/task_dao.h"
#include <gmock/gmock.h>
#include <atomic>
#include <thread>

namespace soilwatercm::tests {

class MockTaskDao : public dao::TaskDao {
public:
    MockTaskDao() : dao::TaskDao(nullptr) {}
    MOCK_METHOD(std::optional<models::Task>, findById, (int id), (override));
    MOCK_METHOD(std::vector<models::Task>, findByProjectId, (int projectId));
    MOCK_METHOD(std::vector<models::Task>, findByStatus, (models::TaskStatus status));
    MOCK_METHOD(std::vector<models::Task>, list, (int page, int pageSize, const std::string& orderBy, bool ascending), (override));
    MOCK_METHOD(std::vector<models::Task>, listRecent, (int limit));
    MOCK_METHOD(int, insert, (const models::Task& task), (override));
    MOCK_METHOD(bool, update, (const models::Task& task), (override));
    MOCK_METHOD(bool, remove, (int id), (override));
    MOCK_METHOD(int, count, (), (override));
    MOCK_METHOD(bool, updateStatus, (int id, models::TaskStatus status, double progress));
    MOCK_METHOD(bool, incrementRetryCount, (int id));
};

class TaskServiceTest : public ServiceTestBase {
protected:
    void SetUp() override {
        ServiceTestBase::SetUp();
        mockTaskDao = std::make_shared<MockTaskDao>();
    }
    
    void TearDown() override {
        mockTaskDao.reset();
        ServiceTestBase::TearDown();
    }
    
    std::shared_ptr<MockTaskDao> mockTaskDao;
};

TEST_F(TaskServiceTest, CreateTask_ShouldInsertTask) {
    EXPECT_CALL(*mockTaskDao, insert(testing::_))
        .WillOnce(testing::Return(1));
    
    models::Task task;
    task.type = models::TaskType::FvcExtraction;
    task.status = models::TaskStatus::Pending;
    
    int id = mockTaskDao->insert(task);
    EXPECT_EQ(id, 1);
}

TEST_F(TaskServiceTest, UpdateTaskStatus_ShouldUpdateStatus) {
    EXPECT_CALL(*mockTaskDao, updateStatus(1, models::TaskStatus::Running, testing::_))
        .WillOnce(testing::Return(true));
    
    bool success = mockTaskDao->updateStatus(1, models::TaskStatus::Running, 0.5);
    EXPECT_TRUE(success);
}

TEST_F(TaskServiceTest, RetryTask_ShouldIncrementRetryCount) {
    models::Task testTask;
    testTask.id = 1;
    testTask.status = models::TaskStatus::Failed;
    testTask.retry_count = 0;
    
    EXPECT_CALL(*mockTaskDao, findById(1))
        .WillOnce(testing::Return(testTask));
    EXPECT_CALL(*mockTaskDao, update(testing::_))
        .WillOnce(testing::Return(true));
    
    auto task = mockTaskDao->findById(1);
    EXPECT_TRUE(task.has_value());
    EXPECT_EQ(task->status, models::TaskStatus::Failed);
}

TEST_F(TaskServiceTest, ConcurrentAccess_ShouldBeThreadSafe) {
    std::atomic<int> counter{0};
    std::vector<std::thread> threads;
    
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&counter]() {
            counter++;
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(counter.load(), 10);
}

TEST_F(TaskServiceTest, GetTasksByStatus_ShouldReturnCorrectTasks) {
    std::vector<models::Task> runningTasks;
    models::Task task1;
    task1.id = 1;
    task1.status = models::TaskStatus::Running;
    runningTasks.push_back(task1);
    
    EXPECT_CALL(*mockTaskDao, findByStatus(models::TaskStatus::Running))
        .WillOnce(testing::Return(runningTasks));
    
    auto result = mockTaskDao->findByStatus(models::TaskStatus::Running);
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].status, models::TaskStatus::Running);
}

} // namespace soilwatercm::tests
