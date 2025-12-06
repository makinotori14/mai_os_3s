#pragma once
#include <vector>

long double sign(int i);
std::vector<std::vector<long double>> minor(
    const std::vector<std::vector<long double>>& a, int skip_row, int skip_col
);
long double det_single(const std::vector<std::vector<long double>>& a);
long double det_parallel(const std::vector<std::vector<long double>>& matrix, int num_threads);