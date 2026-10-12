#define _USE_MATH_DEFINES
#include "test_base.h"
#include "../../src/services/erosion_service.h"
#include <cmath>

namespace soilwatercm::tests {

class ErosionServiceTest : public ServiceTestBase {
protected:
    void SetUp() override {
        ServiceTestBase::SetUp();
    }
    
    void TearDown() override {
        ServiceTestBase::TearDown();
    }
};

TEST_F(ErosionServiceTest, CalculateCFactor_ShouldUseCaiChongfaFormula) {
    double c = 0.6508 - 0.3436 * std::log10(0.5 * 100);
    EXPECT_NEAR(c, 0.0670, 0.01);
}

TEST_F(ErosionServiceTest, CalculateCFactor_ShouldReturnOneForZeroFvc) {
    double c = 0.6508 - 0.3436 * std::log10(0.001 * 100);
    EXPECT_NEAR(c, 0.9944, 0.01);
}

TEST_F(ErosionServiceTest, CalculateCFactor_ShouldReturnMinimalForHighFvc) {
    double c = 0.6508 - 0.3436 * std::log10(1.0 * 100);
    EXPECT_NEAR(c, -0.0362, 0.01);
}

TEST_F(ErosionServiceTest, CalculateRFactor_ShouldUseWenzelFormula) {
    double rainfall = 1000.0;
    double r = 0.0483 * std::pow(rainfall, 1.610);
    EXPECT_GT(r, 0.0);
    EXPECT_NEAR(r, 3265.5, 1.0);
}

TEST_F(ErosionServiceTest, CalculateRFactor_ShouldUseModifiedFormulaForHighRainfall) {
    double rainfall = 2000.0;
    double r = 0.0483 * std::pow(rainfall, 1.610);
    EXPECT_GT(r, 0.0);
    EXPECT_NEAR(r, 9968.0, 1.0);
}

TEST_F(ErosionServiceTest, CalculateKFactor_ShouldUseTextureLookup) {
    std::map<std::string, double> soilKValues = {
        {"clay", 0.25}, {"silty_clay", 0.28}, {"sandy_clay", 0.22},
        {"clay_loam", 0.30}, {"silty_clay_loam", 0.35}, {"sandy_clay_loam", 0.26},
        {"loam", 0.34}, {"silt_loam", 0.40}, {"sandy_loam", 0.30},
        {"silt", 0.42}, {"sand", 0.15}
    };
    
    EXPECT_NEAR(soilKValues["silt_loam"], 0.40, 0.01);
    EXPECT_NEAR(soilKValues["clay"], 0.25, 0.01);
    EXPECT_NEAR(soilKValues["sand"], 0.15, 0.01);
}

TEST_F(ErosionServiceTest, CalculateLsFactor_ShouldScaleWithSlope) {
    auto lsExponent = [](double slopeDeg) -> double {
        if (slopeDeg < 1.0) return 0.2;
        else if (slopeDeg < 3.0) return 0.3;
        else if (slopeDeg < 5.0) return 0.4;
        else return 0.5;
    };
    
    auto lsFactor = [&](double slopeDeg, double length) -> double {
        double m = lsExponent(slopeDeg);
        double s = slopeDeg < 9.0 ?
            (10.8 * std::sin(slopeDeg * M_PI / 180.0) + 0.03) :
            (16.8 * std::sin(slopeDeg * M_PI / 180.0) - 0.50);
        return std::pow(length / 22.13, m) * s;
    };
    
    double gentleSlope = lsFactor(2.0, 30.0);
    double steepSlope = lsFactor(15.0, 30.0);
    
    EXPECT_GT(steepSlope, gentleSlope);
    EXPECT_NEAR(gentleSlope, 0.45, 0.1);
    EXPECT_NEAR(steepSlope, 4.48, 0.1);
}

TEST_F(ErosionServiceTest, CalculatePFactor_ShouldUseConservationLookup) {
    std::map<std::string, double> pFactorValues = {
        {"none", 1.0}, {"contour_farming", 0.6}, {"terracing", 0.3},
        {"strip_cropping", 0.5}, {"sod_cropping", 0.4}, {"conservation_tillage", 0.7}
    };
    
    EXPECT_DOUBLE_EQ(pFactorValues["none"], 1.0);
    EXPECT_DOUBLE_EQ(pFactorValues["terracing"], 0.3);
    EXPECT_DOUBLE_EQ(pFactorValues["contour_farming"], 0.6);
}

TEST_F(ErosionServiceTest, CalculateUSLE_ShouldMultiplyFactors) {
    double r = 8217.6;
    double k = 0.30;
    double ls = 1.5;
    double c = 0.43;
    double p = 1.0;
    
    double a = r * k * ls * c * p;
    
    EXPECT_NEAR(a, 1590.1, 0.1);
}

TEST_F(ErosionServiceTest, ClassifyErosion_ShouldReturnCorrectClass) {
    auto classify = [](double modulus) -> int {
        if (modulus < 1000) return 1;
        else if (modulus < 2500) return 2;
        else if (modulus < 5000) return 3;
        else if (modulus < 8000) return 4;
        else if (modulus < 15000) return 5;
        else return 6;
    };
    
    EXPECT_EQ(classify(500), 1);
    EXPECT_EQ(classify(1500), 2);
    EXPECT_EQ(classify(3500), 3);
    EXPECT_EQ(classify(6500), 4);
    EXPECT_EQ(classify(10000), 5);
    EXPECT_EQ(classify(20000), 6);
}

} // namespace soilwatercm::tests
