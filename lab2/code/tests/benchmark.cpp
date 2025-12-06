#include <gtest/gtest.h>
#include <chrono>
#include <random>
#include <vector>
#include <iostream>
#include <iomanip>
#include "../src/det.hpp"

std::vector<std::vector<long double>> generateMatrix(size_t n, int seed = 0) {
    std::mt19937 gen(seed);
    std::uniform_int_distribution<int> dis(-5, 5);
    std::vector<std::vector<long double>> mat(n, std::vector<long double>(n));
    for (size_t i = 0; i < n; ++i)
        for (size_t j = 0; j < n; ++j)
            mat[i][j] = static_cast<long double>(dis(gen));
    return mat;
}

template<typename Func>
auto measure_time(Func&& f) {
    auto start = std::chrono::high_resolution_clock::now();
    auto result = f();
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    return std::make_pair(result, ms);
}

void runBenchmarkForN(size_t n, int seed = 42) {
    auto matrix = generateMatrix(n, seed);

    auto [serial_res, serial_time] = measure_time([&]() {
        return det_single(matrix);
    });

    std::cout << "n = " << n << " | serial: "
              << serial_time << " ms | det = " << serial_res << "\n";

    std::vector<int> thread_counts = {1, 2, 4, 8};
    for (int k : thread_counts) {
        if (static_cast<size_t>(k) > n) continue;

        auto [par_res, par_time] = measure_time([&]() {
            return det_parallel(matrix, k);
        });

        const long double eps = 1e-9L;
        if (std::abs(par_res - serial_res) > eps) {
            FAIL() << "Result mismatch for n=" << n << ", k=" << k;
        }

        double speedup = (par_time > 0) ? static_cast<double>(serial_time) / par_time : 0.0;
        std::cout << "  k = " << k
                  << " → " << par_time << " ms (speedup: "
                  << std::fixed << std::setprecision(4) << speedup << "x)\n";
    }
    std::cout << "\n";
}

TEST(DeterminantBenchmark, PerformanceAllSizes) {
    std::cout << "Benchmark (n = 1 - 8)\n\n";
    for (size_t n = 1; n <= 8; ++n) {
        runBenchmarkForN(n, 42);
    }
}