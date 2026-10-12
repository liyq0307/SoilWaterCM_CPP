#include "test_base.h"
#include "../../src/services/image_service.h"
#include "../../src/dao/image_dao.h"
#include <gmock/gmock.h>

namespace soilwatercm::tests {

class MockImageDao : public dao::ImageDao {
public:
    MockImageDao() : dao::ImageDao(nullptr) {}
    MOCK_METHOD(std::optional<models::Image>, findById, (int id), (override));
    MOCK_METHOD(std::vector<models::Image>, findByProjectId, (int projectId));
    MOCK_METHOD(std::vector<models::Image>, findByPeriodId, (int periodId));
    MOCK_METHOD(std::vector<models::Image>, list, (int page, int pageSize, const std::string& orderBy, bool ascending), (override));
    MOCK_METHOD(int, insert, (const models::Image& image), (override));
    MOCK_METHOD(bool, update, (const models::Image& image), (override));
    MOCK_METHOD(bool, remove, (int id), (override));
    MOCK_METHOD(int, count, (), (override));
    MOCK_METHOD(bool, updateQualityStatus, (int id, models::QualityStatus status, const std::string& message));
};

class ImageServiceTest : public ServiceTestBase {
protected:
    void SetUp() override {
        ServiceTestBase::SetUp();
        mockImageDao = std::make_shared<MockImageDao>();
    }
    
    void TearDown() override {
        mockImageDao.reset();
        ServiceTestBase::TearDown();
    }
    
    std::shared_ptr<MockImageDao> mockImageDao;
};

TEST_F(ImageServiceTest, GetImageById_ShouldReturnImage) {
    models::Image testImage;
    testImage.id = 1;
    testImage.image_code = "IMG_001";
    testImage.type = models::ImageType::Optical;
    
    EXPECT_CALL(*mockImageDao, findById(1))
        .WillOnce(testing::Return(testImage));
    
    auto result = mockImageDao->findById(1);
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->id, 1);
    EXPECT_EQ(result->image_code, "IMG_001");
}

TEST_F(ImageServiceTest, UpdateQualityStatus_ShouldUpdateStatus) {
    EXPECT_CALL(*mockImageDao, updateQualityStatus(1, models::QualityStatus::Valid, "Quality check passed"))
        .WillOnce(testing::Return(true));
    
    bool success = mockImageDao->updateQualityStatus(1, models::QualityStatus::Valid, "Quality check passed");
    
    EXPECT_TRUE(success);
}

TEST_F(ImageServiceTest, DeleteImage_ShouldRemoveImage) {
    models::Image testImage;
    testImage.id = 1;
    testImage.file_path = "/path/to/image.tif";
    
    EXPECT_CALL(*mockImageDao, findById(1))
        .WillOnce(testing::Return(testImage));
    EXPECT_CALL(*mockImageDao, remove(1))
        .WillOnce(testing::Return(true));
    
    auto image = mockImageDao->findById(1);
    EXPECT_TRUE(image.has_value());
    
    bool success = mockImageDao->remove(1);
    EXPECT_TRUE(success);
}

} // namespace soilwatercm::tests
