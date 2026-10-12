cmake_minimum_required(VERSION 3.20)
project(SoilWaterCM_Tests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package(GTest REQUIRED)
find_package(benchmark REQUIRED)

enable_testing()

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${GTEST_INCLUDE_DIRS}
)

file(GLOB_RECURSE UNIT_TEST_SOURCES CONFIGURE_DEPENDS
    "unit/*.cpp"
)

file(GLOB_RECURSE INTEGRATION_TEST_SOURCES CONFIGURE_DEPENDS
    "integration/*.cpp"
)

file(GLOB_RECURSE BENCHMARK_SOURCES CONFIGURE_DEPENDS
    "benchmark/*.cpp"
)

# Common test dependencies
set(TEST_COMMON_LIBS
    GTest::GTest
    GTest::gmock
    GTest::Main
    GDAL::GDAL
    Drogon::Drogon
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    libpqxx::pqxx
    mongo::mongocxx_shared
    hiredis::hiredis
    jwt-cpp::jwt-cpp
    Boost::filesystem
    Boost::system
    Boost::thread
    OpenSSL::SSL
    OpenSSL::Crypto
)

if(OpenCV_FOUND)
    list(APPEND TEST_COMMON_LIBS ${OpenCV_LIBS})
endif()
if(Eigen3_FOUND)
    list(APPEND TEST_COMMON_LIBS Eigen3::Eigen)
endif()

if(MSVC)
    set(TEST_CXX20_FLAGS /std:c++20)
endif()

add_executable(unit_tests
    ${UNIT_TEST_SOURCES}
)

if(MSVC)
    target_compile_options(unit_tests PRIVATE /std:c++20)
    target_link_options(unit_tests PRIVATE /FORCE:MULTIPLE)
endif()

target_link_libraries(unit_tests
    soilwatercm
    ${TEST_COMMON_LIBS}
)

# Copy soilwatercm DLL/SO next to the test executable so it's found at runtime
add_custom_command(TARGET unit_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:soilwatercm>
        $<TARGET_FILE_DIR:unit_tests>
)

add_test(NAME UnitTests COMMAND unit_tests)

add_executable(integration_tests
    ${INTEGRATION_TEST_SOURCES}
)

if(MSVC)
    target_compile_options(integration_tests PRIVATE /std:c++20)
    target_link_options(integration_tests PRIVATE /FORCE:MULTIPLE)
endif()

target_link_libraries(integration_tests
    soilwatercm
    ${TEST_COMMON_LIBS}
)

add_custom_command(TARGET integration_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:soilwatercm>
        $<TARGET_FILE_DIR:integration_tests>
)

add_test(NAME IntegrationTests COMMAND integration_tests)

add_executable(benchmark_tests
    ${BENCHMARK_SOURCES}
)

if(MSVC)
    target_compile_options(benchmark_tests PRIVATE /std:c++20)
endif()

target_link_libraries(benchmark_tests
    benchmark::benchmark
    ${TEST_COMMON_LIBS}
)
