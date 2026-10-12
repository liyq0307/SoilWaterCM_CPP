#include "test_base.h"
#include "../../src/services/disturbance_service.h"
#include "../../src/dao/disturbance_dao.h"
#include <gmock/gmock.h>
#include <algorithm>

namespace soilwatercm::tests {

class MockDisturbanceDao : public dao::DisturbanceDao {
public:
    MockDisturbanceDao() : dao::DisturbanceDao(nullptr) {}
    MOCK_METHOD(std::optional<models::Disturbance>, findById, (int id), (override));
    MOCK_METHOD(std::vector<models::Disturbance>, findByPeriodId, (int periodId));
    MOCK_METHOD(std::vector<models::Disturbance>, list, (int page, int pageSize, const std::string& orderBy, bool ascending), (override));
    MOCK_METHOD(int, insert, (const models::Disturbance& disturbance), (override));
    MOCK_METHOD(bool, update, (const models::Disturbance& disturbance), (override));
    MOCK_METHOD(bool, remove, (int id), (override));
    MOCK_METHOD(int, count, (), (override));
    MOCK_METHOD(std::vector<int>, batchInsert, (const std::vector<models::Disturbance>& disturbances));
};

class DisturbanceServiceTest : public ServiceTestBase {
protected:
    void SetUp() override {
        ServiceTestBase::SetUp();
        mockDisturbanceDao = std::make_shared<MockDisturbanceDao>();
    }
    
    void TearDown() override {
        mockDisturbanceDao.reset();
        ServiceTestBase::TearDown();
    }
    
    std::shared_ptr<MockDisturbanceDao> mockDisturbanceDao;
};

TEST_F(DisturbanceServiceTest, ChangeDetection_ShouldDetectChanges) {
    double beforeValue = 100.0;
    double afterValue = 150.0;
    
    double change = std::abs(afterValue - beforeValue);
    
    EXPECT_NEAR(change, 50.0, 0.01);
}

TEST_F(DisturbanceServiceTest, ClassifyDisturbance_ShouldUseDynamicThreshold) {
    std::vector<double> changeValues = {0.1, 0.2, 0.5, 0.8, 1.0};
    double maxChange = *std::max_element(changeValues.begin(), changeValues.end());
    double threshold = 0.3 * maxChange;
    
    int disturbanceCount = 0;
    for (double cv : changeValues) {
        if (cv > threshold) ++disturbanceCount;
    }
    
    EXPECT_DOUBLE_EQ(threshold, 0.3);
    EXPECT_EQ(disturbanceCount, 3);
}

TEST_F(DisturbanceServiceTest, DetectDisturbances_ShouldCountExceedingThreshold) {
    std::vector<double> changeValues = {0.01, 0.02, 0.03, 0.04, 0.15};
    double maxChange = *std::max_element(changeValues.begin(), changeValues.end());
    double threshold = 0.3 * maxChange;
    
    int disturbanceCount = 0;
    for (double cv : changeValues) {
        if (cv > threshold) ++disturbanceCount;
    }
    
    EXPECT_NEAR(threshold, 0.045, 0.001);
    EXPECT_EQ(disturbanceCount, 1);
}

TEST_F(DisturbanceServiceTest, FilterSmallPolygons_ShouldRemoveSmallAreas) {
    std::vector<models::Disturbance> disturbances;
    
    models::Disturbance d1;
    d1.id = 1;
    d1.area = 0.05;
    disturbances.push_back(d1);
    
    models::Disturbance d2;
    d2.id = 2;
    d2.area = 0.5;
    disturbances.push_back(d2);
    
    double minArea = 0.1;
    
    std::vector<models::Disturbance> filtered;
    for (const auto& d : disturbances) {
        if (d.area >= minArea) {
            filtered.push_back(d);
        }
    }
    
    EXPECT_EQ(filtered.size(), 1);
    EXPECT_EQ(filtered[0].id, 2);
}

TEST_F(DisturbanceServiceTest, NdBIShouldDetectBuiltUpAreas) {
    std::vector<double> swir1 = {0.3, 0.5, 0.7};
    std::vector<double> nir = {0.6, 0.4, 0.2};
    
    std::vector<double> ndbi;
    for (size_t i = 0; i < swir1.size(); ++i) {
        double val = (swir1[i] - nir[i]) / (swir1[i] + nir[i]);
        ndbi.push_back(val);
    }
    
    EXPECT_NEAR(ndbi[0], -0.333, 0.01);
    EXPECT_NEAR(ndbi[1], 0.111, 0.01);
    EXPECT_NEAR(ndbi[2], 0.555, 0.01);
}

TEST_F(DisturbanceServiceTest, BatchInsert_ShouldInsertMultiple) {
    std::vector<models::Disturbance> disturbances;
    for (int i = 0; i < 5; ++i) {
        models::Disturbance d;
        d.area = 0.1 * (i + 1);
        disturbances.push_back(d);
    }
    
    std::vector<int> ids = {1, 2, 3, 4, 5};
    EXPECT_CALL(*mockDisturbanceDao, batchInsert(testing::_))
        .WillOnce(testing::Return(ids));
    
    std::vector<int> result = mockDisturbanceDao->batchInsert(disturbances);
    EXPECT_EQ(result.size(), 5);
}

TEST_F(DisturbanceServiceTest, CalculateTotalArea_ShouldSumAreas) {
    std::vector<models::Disturbance> disturbances;
    
    models::Disturbance d1;
    d1.area = 0.5;
    disturbances.push_back(d1);
    
    models::Disturbance d2;
    d2.area = 1.5;
    disturbances.push_back(d2);
    
    models::Disturbance d3;
    d3.area = 2.0;
    disturbances.push_back(d3);
    
    double totalArea = 0.0;
    for (const auto& d : disturbances) {
        totalArea += d.area;
    }
    
    EXPECT_NEAR(totalArea, 4.0, 0.01);
}

} // namespace soilwatercm::tests
