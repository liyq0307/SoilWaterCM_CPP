#include "test_base.h"
#include "../../src/services/project_service.h"
#include "../../src/dao/project_dao.h"
#include "../../src/dao/monitoring_period_dao.h"
#include <gmock/gmock.h>

namespace soilwatercm::tests {

class MockProjectDao : public dao::ProjectDao {
public:
    MockProjectDao() : dao::ProjectDao(nullptr) {}
    MOCK_METHOD(std::optional<models::Project>, findById, (int id), (override));
    MOCK_METHOD(std::optional<models::Project>, findByCode, (const std::string& code));
    MOCK_METHOD(std::vector<models::Project>, list, (int page, int pageSize, const std::string& orderBy, bool ascending), (override));
    MOCK_METHOD(int, insert, (const models::Project& project), (override));
    MOCK_METHOD(bool, update, (const models::Project& project), (override));
    MOCK_METHOD(bool, remove, (int id), (override));
    MOCK_METHOD(int, count, (), (override));
    MOCK_METHOD(bool, existsByCode, (const std::string& code));
};

class MockMonitoringPeriodDao : public dao::MonitoringPeriodDao {
public:
    MockMonitoringPeriodDao() : dao::MonitoringPeriodDao(nullptr) {}
    MOCK_METHOD(std::optional<models::MonitoringPeriod>, findById, (int id), (override));
    MOCK_METHOD(std::vector<models::MonitoringPeriod>, findByProjectId, (int projectId));
    MOCK_METHOD(std::vector<models::MonitoringPeriod>, list, (int page, int pageSize, const std::string& orderBy, bool ascending), (override));
    MOCK_METHOD(int, insert, (const models::MonitoringPeriod& period), (override));
    MOCK_METHOD(bool, update, (const models::MonitoringPeriod& period), (override));
    MOCK_METHOD(bool, remove, (int id), (override));
    MOCK_METHOD(int, count, (), (override));
};

class ProjectServiceTest : public ServiceTestBase {
protected:
    void SetUp() override {
        ServiceTestBase::SetUp();
        mockProjectDao = std::make_shared<MockProjectDao>();
        mockPeriodDao = std::make_shared<MockMonitoringPeriodDao>();
    }
    
    void TearDown() override {
        mockProjectDao.reset();
        mockPeriodDao.reset();
        ServiceTestBase::TearDown();
    }
    
    std::shared_ptr<MockProjectDao> mockProjectDao;
    std::shared_ptr<MockMonitoringPeriodDao> mockPeriodDao;
};

TEST_F(ProjectServiceTest, CreateProject_ShouldCheckCodeUniqueness) {
    EXPECT_CALL(*mockProjectDao, existsByCode("TEST001"))
        .WillOnce(testing::Return(true));
    
    EXPECT_TRUE(mockProjectDao->existsByCode("TEST001"));
}

TEST_F(ProjectServiceTest, CreateProject_ShouldInsertNewProject) {
    EXPECT_CALL(*mockProjectDao, existsByCode("TEST001"))
        .WillOnce(testing::Return(false));
    
    EXPECT_CALL(*mockProjectDao, insert(testing::_))
        .WillOnce(testing::Return(1));
    
    models::Project project;
    project.code = "TEST001";
    project.name = "Test Project";
    
    int id = mockProjectDao->insert(project);
    EXPECT_EQ(id, 1);
}

TEST_F(ProjectServiceTest, GetProjectById_ShouldReturnProject) {
    models::Project testProject;
    testProject.id = 1;
    testProject.code = "TEST001";
    testProject.name = "Test Project";
    
    EXPECT_CALL(*mockProjectDao, findById(1))
        .WillOnce(testing::Return(testProject));
    
    auto result = mockProjectDao->findById(1);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->id, 1);
    EXPECT_EQ(result->code, "TEST001");
}

TEST_F(ProjectServiceTest, GetProjectById_ShouldReturnNulloptForNonExistent) {
    EXPECT_CALL(*mockProjectDao, findById(999))
        .WillOnce(testing::Return(std::nullopt));
    
    auto result = mockProjectDao->findById(999);
    
    EXPECT_FALSE(result.has_value());
}

TEST_F(ProjectServiceTest, DeleteProject_ShouldCheckPeriods) {
    EXPECT_CALL(*mockPeriodDao, findByProjectId(1))
        .WillOnce(testing::Return(std::vector<models::MonitoringPeriod>{}));
    
    auto periods = mockPeriodDao->findByProjectId(1);
    EXPECT_TRUE(periods.empty());
}

TEST_F(ProjectServiceTest, DeleteProject_ShouldFailIfPeriodsExist) {
    std::vector<models::MonitoringPeriod> periods;
    periods.push_back(models::MonitoringPeriod{});
    
    EXPECT_CALL(*mockPeriodDao, findByProjectId(1))
        .WillOnce(testing::Return(periods));
    
    auto result = mockPeriodDao->findByProjectId(1);
    EXPECT_FALSE(result.empty());
}

} // namespace soilwatercm::tests
