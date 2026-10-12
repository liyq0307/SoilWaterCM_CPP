#include <benchmark/benchmark.h>
#include <vector>
#include <random>
#include <algorithm>
#include <cmath>
#include <thread>
#include <atomic>

static void BM_RasterCalculation(benchmark::State& state) {
    std::vector<double> rasterData(state.range(0));
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 100.0);
    
    for (auto& val : rasterData) {
        val = dis(gen);
    }
    
    for (auto _ : state) {
        for (double& val : rasterData) {
            val = val * 2.0 + 1.0;
            benchmark::DoNotOptimize(val);
        }
    }
    
    state.SetItemsProcessed(state.range(0));
}
BENCHMARK(BM_RasterCalculation)->Range(1<<10, 1<<20);

static void BM_SpatialQuery(benchmark::State& state) {
    std::vector<std::pair<double, double>> points(state.range(0));
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 180.0);
    
    for (auto& point : points) {
        point = {dis(gen), dis(gen)};
    }
    
    for (auto _ : state) {
        for (const auto& point : points) {
            double distance = std::sqrt(point.first * point.first + point.second * point.second);
            benchmark::DoNotOptimize(distance);
        }
    }
    
    state.SetItemsProcessed(state.range(0));
}
BENCHMARK(BM_SpatialQuery)->Range(1<<10, 1<<18);

static void BM_ConcurrentProcessing(benchmark::State& state) {
    int numThreads = static_cast<int>(state.range(0));
    std::atomic<int> counter{0};
    
    for (auto _ : state) {
        std::vector<std::thread> threads;
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back([&counter]() {
                for (int j = 0; j < 1000; ++j) {
                    counter.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        counter.store(0);
    }
}
BENCHMARK(BM_ConcurrentProcessing)->RangeMultiplier(2)->Range(1, 8);

static void BM_MatrixMultiplication(benchmark::State& state) {
    int size = static_cast<int>(state.range(0));
    std::vector<std::vector<double>> A(size, std::vector<double>(size, 1.0));
    std::vector<std::vector<double>> B(size, std::vector<double>(size, 2.0));
    std::vector<std::vector<double>> C(size, std::vector<double>(size, 0.0));
    
    for (auto _ : state) {
        for (int i = 0; i < size; ++i) {
            for (int j = 0; j < size; ++j) {
                C[i][j] = 0.0;
                for (int k = 0; k < size; ++k) {
                    C[i][j] += A[i][k] * B[k][j];
                }
            }
        }
        benchmark::DoNotOptimize(C);
    }
}
BENCHMARK(BM_MatrixMultiplication)->Range(10, 100);

static void BM_PhaseUnwrapping(benchmark::State& state) {
    std::vector<double> phaseData(state.range(0));
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-3.14159, 3.14159);
    
    for (auto& val : phaseData) {
        val = dis(gen);
    }
    
    for (auto _ : state) {
        for (double& val : phaseData) {
            while (val > 3.14159) val -= 2 * 3.14159;
            while (val < -3.14159) val += 2 * 3.14159;
            benchmark::DoNotOptimize(val);
        }
    }
    
    state.SetItemsProcessed(state.range(0));
}
BENCHMARK(BM_PhaseUnwrapping)->Range(1<<10, 1<<18);

BENCHMARK_MAIN();
