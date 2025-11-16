#include "det.hpp"

int sign(size_t x) {
    return (x % 2 == 0 ? 1 : -1);
}

std::vector<std::vector<int>> minor(const std::vector<std::vector<int>> &a, size_t ei, size_t ej) {
    std::vector<std::vector<int>> ans(a.size() - 1);
    size_t cur = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        if (i == ei) {
            continue;
        }
        for (size_t j = 0; j < a.size(); ++j) {
            if (j == ej) {
                continue;
            }
            ans[cur].push_back(a[i][j]);
        }
        ++cur;
    }
    return ans;
}

int det(const std::vector<std::vector<int>> &a) {
    if (a.size() == 0) {
        return 1;
    }
    if (a.size() == 1) {
        return a[0][0];
    }
    if (a.size() == 2) {
        return a[0][0] * a[1][1] - a[0][1] * a[1][0];
    }

    int ans = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        ans += sign(i) * a[i][0] * det(minor(a, i, 0));
    }
    return ans;
}
