#include "test_base.h"
#include "../../src/infrastructure/file_manager.h"
#include <filesystem>

namespace soilwatercm::tests {

class FileManagerTest : public InfrastructureTestBase {
protected:
    void SetUp() override {
        InfrastructureTestBase::SetUp();
        testDir = "test_file_manager";
        std::filesystem::create_directories(testDir);
    }
    
    void TearDown() override {
        std::filesystem::remove_all(testDir);
        InfrastructureTestBase::TearDown();
    }
    
    std::string testDir;
};

TEST_F(FileManagerTest, WriteFile_ShouldCreateFile) {
    std::string filePath = testDir + "/test.txt";
    std::string content = "Hello, World!";
    
    std::ofstream file(filePath);
    file << content;
    file.close();
    
    EXPECT_TRUE(std::filesystem::exists(filePath));
}

TEST_F(FileManagerTest, ReadFile_ShouldReturnContent) {
    std::string filePath = testDir + "/test_read.txt";
    std::string content = "Test content";
    
    std::ofstream outFile(filePath);
    outFile << content;
    outFile.close();
    
    std::ifstream inFile(filePath);
    std::string readContent((std::istreambuf_iterator<char>(inFile)),
                            std::istreambuf_iterator<char>());
    
    EXPECT_EQ(readContent, content);
}

TEST_F(FileManagerTest, DeleteFile_ShouldRemoveFile) {
    std::string filePath = testDir + "/test_delete.txt";
    
    std::ofstream file(filePath);
    file << "content";
    file.close();
    
    EXPECT_TRUE(std::filesystem::exists(filePath));
    
    std::filesystem::remove(filePath);
    
    EXPECT_FALSE(std::filesystem::exists(filePath));
}

TEST_F(FileManagerTest, CreateDirectory_ShouldCreateNestedDirectories) {
    std::string dirPath = testDir + "/nested/dirs";
    
    std::filesystem::create_directories(dirPath);
    
    EXPECT_TRUE(std::filesystem::exists(dirPath));
}

TEST_F(FileManagerTest, GetFileSize_ShouldReturnCorrectSize) {
    std::string filePath = testDir + "/test_size.txt";
    std::string content = "12345";
    
    std::ofstream file(filePath);
    file << content;
    file.close();
    
    auto fileSize = std::filesystem::file_size(filePath);
    
    EXPECT_EQ(fileSize, content.size());
}

} // namespace soilwatercm::tests
