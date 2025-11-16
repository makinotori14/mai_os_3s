#include <iostream>
#include <thread>
#include <algorithm>
#include "det.hpp"

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

    std::cout << det(a) << std::endl;
    return 0;
}