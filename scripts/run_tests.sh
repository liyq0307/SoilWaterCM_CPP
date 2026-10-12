#!/bin/bash

set -e

echo "========================================"
echo "SoilWaterCM C++ Test Runner"
echo "========================================"

BUILD_DIR="build"
TEST_OUTPUT_DIR="test_results"

mkdir -p $TEST_OUTPUT_DIR

echo ""
echo "Running Unit Tests..."
echo "----------------------------------------"
./$BUILD_DIR/tests/unit/unit_tests --gtest_output=xml:$TEST_OUTPUT_DIR/unit_tests.xml

echo ""
echo "Running Integration Tests..."
echo "----------------------------------------"
./$BUILD_DIR/tests/integration/integration_tests --gtest_output=xml:$TEST_OUTPUT_DIR/integration_tests.xml

echo ""
echo "Running Benchmark Tests..."
echo "----------------------------------------"
./$BUILD_DIR/tests/benchmark/benchmark_tests --benchmark_out=$TEST_OUTPUT_DIR/benchmark_results.json

echo ""
echo "========================================"
echo "All tests completed successfully!"
echo "Results saved to: $TEST_OUTPUT_DIR"
echo "========================================"
