#!/bin/bash

set -e

echo "========================================"
echo "Code Quality Check"
echo "========================================"

PROJECT_ROOT=$(pwd)
SOURCE_DIR="$PROJECT_ROOT/src"
REPORT_DIR="$PROJECT_ROOT/quality_reports"

mkdir -p $REPORT_DIR

echo ""
echo "Running Clang-Tidy..."
echo "----------------------------------------"
find $SOURCE_DIR -name "*.cpp" -o -name "*.h" | while read file; do
    clang-tidy $file -- -std=c++17 2>&1 | tee -a $REPORT_DIR/clang-tidy.log
done

echo ""
echo "Running Clang-Format Check..."
echo "----------------------------------------"
find $SOURCE_DIR -name "*.cpp" -o -name "*.h" | while read file; do
    if ! clang-format --dry-run --Werror $file; then
        echo "Format issue in: $file" | tee -a $REPORT_DIR/clang-format.log
    fi
done

echo ""
echo "Running Cppcheck..."
echo "----------------------------------------"
cppcheck --enable=all --std=c++17 $SOURCE_DIR 2>&1 | tee $REPORT_DIR/cppcheck.log

echo ""
echo "========================================"
echo "Code quality check completed!"
echo "Reports saved to: $REPORT_DIR"
echo "========================================"
