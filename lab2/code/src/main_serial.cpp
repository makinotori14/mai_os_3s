#include <iostream>
#include <vector>
#include "det.hpp"

int main() {
    int n;
    std::cout << "Введите размер матрицы: ";
    std::cin >> n;

    std::vector<std::vector<long double>> a(n, std::vector<long double>(n));
    std::cout << "Введите элементы матрицы " << n << "x" << n << ":\n";
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            std::cin >> a[i][j];
        }
    }

    long double result = det_single(a);
    std::cout << "Определитель (serial) = " << result << std::endl;
    return 0;
}