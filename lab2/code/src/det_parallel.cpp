#include <iostream>
#include <vector>
#include <pthread.h>
#include "det.hpp"

struct ThreadData {
    int n;
    int start_row;
    int end_row;
    const std::vector<std::vector<long double>>& matrix;
    long double result;
    int thread_id;

    ThreadData(int sz, int start, int end,
               const std::vector<std::vector<long double>>& mat, int id)
        : n(sz), start_row(start), end_row(end), matrix(mat), result(0.0L), thread_id(id) {}
};

void* thread_worker(void* arg) {
    ThreadData* data = static_cast<ThreadData*>(arg);
    data->result = 0.0L;
    for (int i = data->start_row; i <= data->end_row; ++i) {
        long double sub_det = det_single(minor(data->matrix, i, 0));
        data->result += sign(i) * data->matrix[i][0] * sub_det;
    }
    return nullptr;
}

long double det_parallel(const std::vector<std::vector<long double>>& matrix, int num_threads) {
    int n = static_cast<int>(matrix.size());
    if (n <= 2 || num_threads <= 1) {
        return det_single(matrix);
    }

    if (num_threads > n) num_threads = n;

    std::vector<pthread_t> threads(num_threads);
    std::vector<ThreadData*> tdata(num_threads);

    int rows_per_thread = n / num_threads;
    int remainder = n % num_threads;
    int current = 0;

    for (int i = 0; i < num_threads; ++i) {
        int extra = (i < remainder) ? 1 : 0;
        int end = current + rows_per_thread + extra - 1;
        tdata[i] = new ThreadData(n, current, end, matrix, i);
        current = end + 1;
    }

    for (int i = 0; i < num_threads; ++i) {
        if (pthread_create(&threads[i], nullptr, thread_worker, tdata[i]) != 0) {
            std::cerr << "Ошибка создания потока " << i << std::endl;
            for (int j = 0; j <= i; ++j) {
                delete tdata[j];
            }
            return det_single(matrix);
        }
    }

    for (int i = 0; i < num_threads; ++i) {
        pthread_join(threads[i], nullptr);
    }

    long double total = 0.0L;
    for (int i = 0; i < num_threads; ++i) {
        total += tdata[i]->result;
        delete tdata[i];
    }

    return total;
}