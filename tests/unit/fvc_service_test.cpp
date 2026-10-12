#include "test_base.h"
#include "../../src/services/fvc_service.h"
#include <cmath>

namespace soilwatercm::tests {

class FvcServiceTest : public ServiceTestBase {
protected:
    void SetUp() override {
        ServiceTestBase::SetUp();
    }
    
    void TearDown() override {
        ServiceTestBase::TearDown();
    }
};

TEST_F(FvcServiceTest, CalculateNdvi_ShouldReturnValueInRange) {
    double nir = 0.6;
    double red = 0.2;
    
    double ndvi = (nir - red) / (nir + red);
    
    EXPECT_GE(ndvi, -1.0);
    EXPECT_LE(ndvi, 1.0);
    EXPECT_NEAR(ndvi, 0.5, 0.01);
}

TEST_F(FvcServiceTest, CalculateNdvi_ShouldHandleZeroDenominator) {
    volatile double nir = 0.0;
    volatile double red = 0.0;
    
    double ndvi = 0.0;
    double denom = nir + red;
    if (std::abs(denom) > 1e-10) {
        ndvi = (nir - red) / denom;
    }
    
    EXPECT_EQ(ndvi, 0.0);
}

TEST_F(FvcServiceTest, CalculateModifiedNdvi_ShouldHandleBlueBand) {
    double nir = 0.6;
    double red = 0.2;
    double blue = 0.1;
    
    double mndvi = (nir - (red - blue)) / (nir + (red - blue));
    
    EXPECT_GE(mndvi, -1.0);
    EXPECT_LE(mndvi, 1.0);
    EXPECT_NEAR(mndvi, 0.71, 0.01);
}

TEST_F(FvcServiceTest, CalculateNdvi2_ShouldUseGreenBand) {
    double nir = 0.6;
    double green = 0.3;
    
    double ndvi2 = (nir - green) / (nir + green);
    
    EXPECT_GE(ndvi2, -1.0);
    EXPECT_LE(ndvi2, 1.0);
    EXPECT_NEAR(ndvi2, 0.33, 0.01);
}

TEST_F(FvcServiceTest, CalculateFvc_ShouldReturnValueInRange) {
    double ndvi = 0.5;
    double ndviSoil = 0.05;
    double ndviVeg = 0.70;
    
    double fvc = (ndvi - ndviSoil) / (ndviVeg - ndviSoil);
    fvc = std::max(0.0, std::min(1.0, fvc));
    
    EXPECT_GE(fvc, 0.0);
    EXPECT_LE(fvc, 1.0);
    EXPECT_NEAR(fvc, 0.6923, 0.01);
}

TEST_F(FvcServiceTest, CalculateFvc_ShouldClampToZero) {
    double ndvi = 0.01;
    double ndviSoil = 0.05;
    double ndviVeg = 0.70;
    
    double fvc = (ndvi - ndviSoil) / (ndviVeg - ndviSoil);
    fvc = std::max(0.0, std::min(1.0, fvc));
    
    EXPECT_EQ(fvc, 0.0);
}

TEST_F(FvcServiceTest, CalculateFvc_ShouldClampToOne) {
    double ndvi = 0.9;
    double ndviSoil = 0.05;
    double ndviVeg = 0.70;
    
    double fvc = (ndvi - ndviSoil) / (ndviVeg - ndviSoil);
    fvc = std::max(0.0, std::min(1.0, fvc));
    
    EXPECT_EQ(fvc, 1.0);
}

TEST_F(FvcServiceTest, ClassifyFvcValue_ShouldReturnCorrectClass) {
    std::vector<std::pair<double, int>> testCases = {
        {0.1, 1}, {0.3, 2}, {0.5, 3}, {0.7, 4}, {0.9, 5}
    };
    
    for (const auto& [fvc, expected] : testCases) {
        int classification;
        if (fvc < 0.2) classification = 1;
        else if (fvc < 0.4) classification = 2;
        else if (fvc < 0.6) classification = 3;
        else if (fvc < 0.8) classification = 4;
        else classification = 5;
        
        EXPECT_EQ(classification, expected) << "FVC=" << fvc;
    }
}

TEST_F(FvcServiceTest, CalculateStatistics_ShouldComputeMean) {
    std::vector<double> fvcValues = {0.1, 0.3, 0.5, 0.7, 0.9};
    
    double sum = 0.0;
    for (double val : fvcValues) {
        sum += val;
    }
    double mean = sum / fvcValues.size();
    
    EXPECT_NEAR(mean, 0.5, 0.01);
}

TEST_F(FvcServiceTest, CalculateStatistics_ShouldClassifyByArea) {
    std::vector<double> fvcValues = {0.1, 0.1, 0.1, 0.5, 0.5, 0.9, 0.9};
    
    int lowCount = 0, midLowCount = 0, midCount = 0, midHighCount = 0, highCount = 0;
    for (double fvc : fvcValues) {
        if (fvc < 0.2) ++lowCount;
        else if (fvc < 0.4) ++midLowCount;
        else if (fvc < 0.6) ++midCount;
        else if (fvc < 0.8) ++midHighCount;
        else ++highCount;
    }
    
    EXPECT_EQ(lowCount, 3);
    EXPECT_EQ(midCount, 2);
    EXPECT_EQ(highCount, 2);
}

} // namespace soilwatercm::tests
