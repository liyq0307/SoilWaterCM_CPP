#include "test_base.h"
#include "../../src/infrastructure/config_manager.h"
#include <filesystem>
#include <fstream>

namespace soilwatercm::tests {

class ConfigManagerTest : public InfrastructureTestBase {
protected:
    void SetUp() override {
        InfrastructureTestBase::SetUp();
        
        testConfigPath = "test_config.yaml";
        std::ofstream file(testConfigPath);
        file << "test_key: test_value\n";
        file << "test_number: 42\n";
        file << "test_bool: true\n";
        file.close();
    }
    
    void TearDown() override {
        std::filesystem::remove(testConfigPath);
        InfrastructureTestBase::TearDown();
    }
    
    std::string testConfigPath;
};

TEST_F(ConfigManagerTest, Load_ShouldParseYamlFile) {
    auto& config = infrastructure::ConfigManager::getInstance();
    
    EXPECT_NO_THROW(config.load(testConfigPath));
}

TEST_F(ConfigManagerTest, GetString_ShouldReturnValue) {
    auto& config = infrastructure::ConfigManager::getInstance();
    config.load(testConfigPath);
    
    std::string value = config.get<std::string>("test_key");
    
    EXPECT_EQ(value, "test_value");
}

TEST_F(ConfigManagerTest, GetInt_ShouldReturnValue) {
    auto& config = infrastructure::ConfigManager::getInstance();
    config.load(testConfigPath);
    
    int value = config.get<int>("test_number");
    
    EXPECT_EQ(value, 42);
}

TEST_F(ConfigManagerTest, GetBool_ShouldReturnValue) {
    auto& config = infrastructure::ConfigManager::getInstance();
    config.load(testConfigPath);
    
    bool value = config.get<bool>("test_bool");
    
    EXPECT_TRUE(value);
}

TEST_F(ConfigManagerTest, GetString_ShouldReturnDefaultForMissingKey) {
    auto& config = infrastructure::ConfigManager::getInstance();
    config.load(testConfigPath);
    
    std::string value = config.get<std::string>("nonexistent_key", "default_value");
    
    EXPECT_EQ(value, "default_value");
}

} // namespace soilwatercm::tests
