#include <iostream>
#include <thread>
#include "det.hpp"
#include <algorithm>

void task(const std::vector<std::vector<int>> &a, int &ans, const std::vector<size_t> &row) {
    for (size_t r : row) {
        ans += sign(r) * a[r][0] * det(minor(a, r, 0)); 
    }
}

int main() {
    size_t k;
    std::cin >> k;

    if (k < 1) {
        std::cerr << "at least 1 :)" << std::endl;
        return 1;
    }

    size_t n;
    std::cin >> n;

    k = std::min(k, n);

    std::vector<std::vector<int>> a(n, std::vector<int>(n));

    for (size_t i = 0; i < n; ++i) {
        for (int &x : a[i]) {
            std::cin >> x;
        }
    }

    if (n <= 2) {
        std::cout << det(a) << std::endl;
        return 0;
    }

    std::vector<std::vector<size_t>> rows(k);

    for (size_t i = 0; i < n; ++i) {
        rows[i % k].push_back(i);
    }

    std::vector<int> part_ans(k);

    std::vector<std::thread> threads;
    for (size_t i = 0; i < k; ++i) {
        threads.emplace_back(task, std::ref(a), std::ref(part_ans[i]), std::ref(rows[i]));
    }
    for (auto &t : threads) {
        t.join();
    }
    int ans = 0;
    for (int x : part_ans) {
        ans += x;
    }

    std::cout << ans << std::endl;
    return 0;
}