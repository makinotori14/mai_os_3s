#include <iostream>
#include <vector>
#include "det.hpp"

int main() {
    int n, threads;
    std::cout << "Введите размер матрицы: ";
    std::cin >> n;
    std::cout << "Введите количество потоков: ";
    std::cin >> threads;

    std::vector<std::vector<long double>> mat(n, std::vector<long double>(n));
    std::cout << "Введите элементы матрицы " << n << "x" << n << ":\n";
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            std::cin >> mat[i][j];
        }
    }

    long double result = det_parallel(mat, threads);
    std::cout << "Определитель (parallel) = " << result << std::endl;
    return 0;
}