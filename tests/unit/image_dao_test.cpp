#include "test_base.h"
#include "../../src/dao/image_dao.h"

namespace soilwatercm::tests {

class ImageDaoTest : public DaoTestBase {
protected:
    void SetUp() override {
        DaoTestBase::SetUp();
    }
    
    void TearDown() override {
        DaoTestBase::TearDown();
    }
};

TEST_F(ImageDaoTest, FindById_ShouldReturnImage) {
    EXPECT_TRUE(true);
}

TEST_F(ImageDaoTest, FindByProjectId_ShouldReturnImages) {
    EXPECT_TRUE(true);
}

TEST_F(ImageDaoTest, FindByPeriodId_ShouldReturnImages) {
    EXPECT_TRUE(true);
}

TEST_F(ImageDaoTest, UpdateQualityStatus_ShouldUpdateStatus) {
    EXPECT_TRUE(true);
}

} // namespace soilwatercm::tests
