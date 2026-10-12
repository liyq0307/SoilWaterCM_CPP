#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>

namespace soilwatercm::tests {

class TestBase : public ::testing::Test {
protected:
    void SetUp() override {
    }
    
    void TearDown() override {
    }
};

class ServiceTestBase : public TestBase {
protected:
    void SetUp() override {
        TestBase::SetUp();
    }
    
    void TearDown() override {
        TestBase::TearDown();
    }
};

class DaoTestBase : public TestBase {
protected:
    void SetUp() override {
        TestBase::SetUp();
    }
    
    void TearDown() override {
        TestBase::TearDown();
    }
};

class InfrastructureTestBase : public TestBase {
protected:
    void SetUp() override {
        TestBase::SetUp();
    }
    
    void TearDown() override {
        TestBase::TearDown();
    }
};

} // namespace soilwatercm::tests
