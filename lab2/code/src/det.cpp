#include "det.hpp"

long double sign(int i) {
    return (i % 2 == 0) ? 1.0L : -1.0L;
}

std::vector<std::vector<long double>> minor(
    const std::vector<std::vector<long double>>& a, int skip_row, int skip_col) {
    int n = static_cast<int>(a.size());
    std::vector<std::vector<long double>> m;
    for (int i = 0; i < n; ++i) {
        if (i == skip_row) continue;
        std::vector<long double> row;
        for (int j = 0; j < n; ++j) {
            if (j == skip_col) continue;
            row.push_back(a[i][j]);
        }
        m.push_back(row);
    }
    return m;
}

long double det_single(const std::vector<std::vector<long double>>& a) {
    int n = static_cast<int>(a.size());
    if (n == 0) return 1.0L;
    if (n == 1) return a[0][0];
    if (n == 2) return a[0][0] * a[1][1] - a[0][1] * a[1][0];

    long double result = 0.0L;
    for (int i = 0; i < n; ++i) {
        long double sub_det = det_single(minor(a, i, 0));
        result += sign(i) * a[i][0] * sub_det;
    }
    return result;
}