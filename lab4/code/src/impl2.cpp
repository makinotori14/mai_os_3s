#include "contracts.h"

extern "C" double E(int x) {
    if (x < 0) {
        return 0.0;
    }
    double result = 0.0;
    double term = 1.0;
    for (int n = 0; n <= x; ++n) {
        if (n > 0) {
            term /= static_cast<double>(n);
        }
        result += term;
    }
    return result;
}

extern "C" double Area(double a, double b) {
    return a * b / 2.0;
}
