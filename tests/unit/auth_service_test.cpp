#include "test_base.h"
#include "../../src/services/auth_service.h"
#include "../../src/dao/user_dao.h"
#include <gmock/gmock.h>

namespace soilwatercm::tests {

class MockUserDao : public dao::UserDao {
public:
    MockUserDao() : dao::UserDao(nullptr) {}
    MOCK_METHOD(std::optional<models::User>, findById, (int id), (override));
    MOCK_METHOD(std::optional<models::User>, findByUsername, (const std::string& username));
    MOCK_METHOD(std::vector<models::User>, list, (int page, int pageSize, const std::string& orderBy, bool ascending), (override));
    MOCK_METHOD(int, insert, (const models::User& user), (override));
    MOCK_METHOD(bool, update, (const models::User& user), (override));
    MOCK_METHOD(bool, remove, (int id), (override));
    MOCK_METHOD(int, count, (), (override));
    MOCK_METHOD(bool, updateLastLogin, (int userId));
    MOCK_METHOD(bool, existsByUsername, (const std::string& username));
};

class AuthServiceTest : public ServiceTestBase {
protected:
    void SetUp() override {
        ServiceTestBase::SetUp();
        
        mockUserDao = std::make_shared<MockUserDao>();
        
        std::string jwtSecret = "test_secret_key_for_testing_purposes_only";
        authService = std::make_shared<services::AuthService>(
            mockUserDao, jwtSecret, 3600, "soilwatercm_test"
        );
    }
    
    void TearDown() override {
        authService.reset();
        mockUserDao.reset();
        ServiceTestBase::TearDown();
    }
    
    std::shared_ptr<MockUserDao> mockUserDao;
    std::shared_ptr<services::AuthService> authService;
};

TEST_F(AuthServiceTest, HashPassword_ShouldReturnNonEmptyHash) {
    std::string password = "test_password_123";
    std::string hash = services::AuthService::hashPassword(password);
    
    EXPECT_FALSE(hash.empty());
    EXPECT_NE(hash, password);
}

TEST_F(AuthServiceTest, VerifyPassword_ShouldReturnTrueForCorrectPassword) {
    std::string password = "test_password_123";
    std::string hash = services::AuthService::hashPassword(password);
    
    bool result = services::AuthService::verifyPassword(password, hash);
    
    EXPECT_TRUE(result);
}

TEST_F(AuthServiceTest, VerifyPassword_ShouldReturnFalseForIncorrectPassword) {
    std::string password = "test_password_123";
    std::string hash = services::AuthService::hashPassword(password);
    std::string wrongPassword = "wrong_password";
    
    bool result = services::AuthService::verifyPassword(wrongPassword, hash);
    
    EXPECT_FALSE(result);
}

TEST_F(AuthServiceTest, GenerateToken_ShouldReturnNonEmptyToken) {
    std::string token = authService->generateToken(1, models::UserRole::Admin);
    
    EXPECT_FALSE(token.empty());
}

TEST_F(AuthServiceTest, ValidateToken_ShouldReturnUserIdForValidToken) {
    std::string token = authService->generateToken(1, models::UserRole::Admin);
    
    auto userIdOpt = authService->validateToken(token);
    
    EXPECT_TRUE(userIdOpt.has_value());
    EXPECT_EQ(*userIdOpt, 1);
}

TEST_F(AuthServiceTest, ValidateToken_ShouldReturnNulloptForInvalidToken) {
    std::string invalidToken = "invalid_token_string";
    
    auto userIdOpt = authService->validateToken(invalidToken);
    
    EXPECT_FALSE(userIdOpt.has_value());
}

TEST_F(AuthServiceTest, Authenticate_ShouldReturnUserForValidCredentials) {
    models::User testUser;
    testUser.id = 1;
    testUser.username = "testuser";
    testUser.password_hash = services::AuthService::hashPassword("password123");
    testUser.is_active = true;
    
    EXPECT_CALL(*mockUserDao, findByUsername(testing::_))
        .WillOnce(testing::Return(testUser));
    EXPECT_CALL(*mockUserDao, updateLastLogin(testing::_))
        .WillOnce(testing::Return(true));
    
    auto result = authService->authenticate("testuser", "password123");
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->id, 1);
}

TEST_F(AuthServiceTest, Authenticate_ShouldReturnNulloptForInvalidPassword) {
    models::User testUser;
    testUser.id = 1;
    testUser.username = "testuser";
    testUser.password_hash = services::AuthService::hashPassword("password123");
    testUser.is_active = true;
    
    EXPECT_CALL(*mockUserDao, findByUsername(testing::_))
        .WillOnce(testing::Return(testUser));
    
    auto result = authService->authenticate("testuser", "wrongpassword");
    
    EXPECT_FALSE(result.has_value());
}

TEST_F(AuthServiceTest, Authenticate_ShouldReturnNulloptForInactiveUser) {
    models::User testUser;
    testUser.id = 1;
    testUser.username = "testuser";
    testUser.password_hash = services::AuthService::hashPassword("password123");
    testUser.is_active = false;
    
    EXPECT_CALL(*mockUserDao, findByUsername(testing::_))
        .WillOnce(testing::Return(testUser));
    
    auto result = authService->authenticate("testuser", "password123");
    
    EXPECT_FALSE(result.has_value());
}

} // namespace soilwatercm::tests
