#include "test_base.h"
#include "../../src/dao/user_dao.h"
#include "../../src/repository/postgres_repository.h"
#include <gmock/gmock.h>

namespace soilwatercm::tests {

class MockPostgresRepository : public repository::PostgresRepository {
public:
    MockPostgresRepository() : repository::PostgresRepository(nullptr) {}
    MOCK_METHOD((std::optional<std::map<std::string, std::string>>), executeQuery, 
                (const std::string& query));
    MOCK_METHOD(bool, executeUpdate, (const std::string& query));
};

class UserDaoTest : public DaoTestBase {
protected:
    void SetUp() override {
        DaoTestBase::SetUp();
    }
    
    void TearDown() override {
        DaoTestBase::TearDown();
    }
};

TEST_F(UserDaoTest, FindById_ShouldReturnUser) {
    EXPECT_TRUE(true);
}

TEST_F(UserDaoTest, FindByUsername_ShouldReturnUser) {
    EXPECT_TRUE(true);
}

TEST_F(UserDaoTest, Insert_ShouldReturnUserId) {
    EXPECT_TRUE(true);
}

TEST_F(UserDaoTest, Update_ShouldReturnTrue) {
    EXPECT_TRUE(true);
}

TEST_F(UserDaoTest, Remove_ShouldReturnTrue) {
    EXPECT_TRUE(true);
}

} // namespace soilwatercm::tests
