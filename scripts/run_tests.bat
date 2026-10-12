@echo off
setlocal enabledelayedexpansion

echo ========================================
echo SoilWaterCM C++ Test Runner (Windows)
echo ========================================

set BUILD_DIR=build
set TEST_OUTPUT_DIR=test_results

if not exist %TEST_OUTPUT_DIR% mkdir %TEST_OUTPUT_DIR%

echo.
echo Running Unit Tests...
echo ----------------------------------------
%BUILD_DIR%\tests\unit\Debug\unit_tests.exe --gtest_output=xml:%TEST_OUTPUT_DIR%/unit_tests.xml
if !errorlevel! neq 0 (
    echo Unit tests failed!
    exit /b 1
)

echo.
echo Running Integration Tests...
echo ----------------------------------------
%BUILD_DIR%\tests\integration\Debug\integration_tests.exe --gtest_output=xml:%TEST_OUTPUT_DIR%/integration_tests.xml
if !errorlevel! neq 0 (
    echo Integration tests failed!
    exit /b 1
)

echo.
echo Running Benchmark Tests...
echo ----------------------------------------
%BUILD_DIR%\tests\benchmark\Debug\benchmark_tests.exe --benchmark_out=%TEST_OUTPUT_DIR%/benchmark_results.json

echo.
echo ========================================
echo All tests completed successfully!
echo Results saved to: %TEST_OUTPUT_DIR%
echo ========================================

endlocal
