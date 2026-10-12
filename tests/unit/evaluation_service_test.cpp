#include "test_base.h"
#include "../../src/services/evaluation_service.h"
#include "../../src/dao/evaluation_dao.h"
#include "../../src/dao/task_dao.h"
#include <gmock/gmock.h>
#include <cmath>

namespace soilwatercm::tests {

class EvaluationServiceTest : public ServiceTestBase {
protected:
    void SetUp() override {
        ServiceTestBase::SetUp();
    }
    
    void TearDown() override {
        ServiceTestBase::TearDown();
    }
};

TEST_F(EvaluationServiceTest, EvaluateCompliance_ShouldReturnExcellentForHighScore) {
    services::EvaluationService service(nullptr, nullptr);
    
    auto level = service.evaluateCompliance(95.0);
    
    EXPECT_EQ(level, models::ComplianceLevel::Excellent);
}

TEST_F(EvaluationServiceTest, EvaluateCompliance_ShouldReturnGoodForMediumHighScore) {
    services::EvaluationService service(nullptr, nullptr);
    
    auto level = service.evaluateCompliance(85.0);
    
    EXPECT_EQ(level, models::ComplianceLevel::Good);
}

TEST_F(EvaluationServiceTest, EvaluateCompliance_ShouldReturnQualifiedForMediumScore) {
    services::EvaluationService service(nullptr, nullptr);
    
    auto level = service.evaluateCompliance(75.0);
    
    EXPECT_EQ(level, models::ComplianceLevel::Qualified);
}

TEST_F(EvaluationServiceTest, EvaluateCompliance_ShouldReturnUnqualifiedForLowScore) {
    services::EvaluationService service(nullptr, nullptr);
    
    auto level = service.evaluateCompliance(60.0);
    
    EXPECT_EQ(level, models::ComplianceLevel::Unqualified);
}

TEST_F(EvaluationServiceTest, EvaluateRisk_ShouldReturnHighForHighRiskScore) {
    services::EvaluationService service(nullptr, nullptr);
    
    auto level = service.evaluateRisk(80.0);
    
    EXPECT_EQ(level, models::RiskLevel::High);
}

TEST_F(EvaluationServiceTest, EvaluateRisk_ShouldReturnMediumForMediumRiskScore) {
    services::EvaluationService service(nullptr, nullptr);
    
    auto level = service.evaluateRisk(50.0);
    
    EXPECT_EQ(level, models::RiskLevel::Medium);
}

TEST_F(EvaluationServiceTest, EvaluateRisk_ShouldReturnLowForLowRiskScore) {
    services::EvaluationService service(nullptr, nullptr);
    
    auto level = service.evaluateRisk(20.0);
    
    EXPECT_EQ(level, models::RiskLevel::Low);
}

TEST_F(EvaluationServiceTest, CombineWeights_ShouldReturnCombinedWeights) {
    services::EvaluationService service(nullptr, nullptr);
    
    std::map<std::string, double> ahpWeights = {
        {"indicator1", 0.4},
        {"indicator2", 0.3},
        {"indicator3", 0.3}
    };
    
    std::map<std::string, double> entropyWeights = {
        {"indicator1", 0.5},
        {"indicator2", 0.3},
        {"indicator3", 0.2}
    };
    
    auto combined = service.combineWeights(ahpWeights, entropyWeights, 0.5);
    
    EXPECT_NEAR(combined["indicator1"], 0.45, 0.01);
    EXPECT_NEAR(combined["indicator2"], 0.30, 0.01);
    EXPECT_NEAR(combined["indicator3"], 0.25, 0.01);
}

} // namespace soilwatercm::tests
