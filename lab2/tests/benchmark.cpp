#include <gtest/gtest.h>
#include <chrono>
#include <random>
#include <vector>
#include <iostream>
#include <algorithm>
#include <thread>
#include <iomanip>
#include "../src/det.hpp"

void task(const std::vector<std::vector<int>> &a, int &ans, const std::vector<size_t> &row) {
    for (size_t r : row) {
        ans += sign(r) * a[r][0] * det(minor(a, r, 0));
    }
}

int parallel_det(const std::vector<std::vector<int>>& a, size_t k_input) {
    size_t n = a.size();
    if (n <= 2) {
        return det(a);
    }

    size_t k = std::min(k_input, n);
    if (k < 1) k = 1;

    std::vector<std::vector<size_t>> rows(k);
    for (size_t i = 0; i < n; ++i) {
        rows[i % k].push_back(i);
    }

    std::vector<int> part_ans(k, 0);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < k; ++i) {
        threads.emplace_back(task, std::ref(a), std::ref(part_ans[i]), std::ref(rows[i]));
    }

    for (auto& t : threads) {
        t.join();
    }

    int total = 0;
    for (int x : part_ans) total += x;
    return total;
}

std::vector<std::vector<int>> generateMatrix(size_t n, int seed = 0) {
    std::mt19937 gen(seed);
    std::uniform_int_distribution<> dis(-3, 3);
    std::vector<std::vector<int>> mat(n, std::vector<int>(n));
    for (size_t i = 0; i < n; ++i)
        for (size_t j = 0; j < n; ++j)
            mat[i][j] = dis(gen);
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
        return det(matrix);
    });

    std::cout << "n = " << n << " serial: " 
              << serial_time << " ms \ndet = " << serial_res << "\n";

    std::vector<size_t> thread_counts = {1, 2, 4, 8};
    for (size_t k : thread_counts) {
        if (k > n) continue;

        auto [par_res, par_time] = measure_time([&]() {
            return parallel_det(matrix, k);
        });

        ASSERT_EQ(par_res, serial_res) << "Result mismatch for n=" << n << ", k=" << k;

        double speedup = static_cast<double>(serial_time) / par_time;
        std::cout << "k = " << k 
                  << " -> " << par_time << " ms (speedup: " 
                  << std::fixed << std::setprecision(5) << speedup << "x)\n";
    }
    std::cout << "\n";
}

TEST(DeterminantBenchmark, PerformanceAllSizes) {
    std::cout << "Benchmark: n = 1...8 (seed = 42)\n";
    std::cout << "\n";

    for (size_t n = 1; n <= 8; ++n) {
        runBenchmarkForN(n, 42);
    }
}