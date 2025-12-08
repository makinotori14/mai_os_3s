#include "contracts.h"
#include <cmath>

extern "C" double E(int x) {
    if (x <= 0) {
        return 0.0;
    }
    double base = 1.0 + 1.0 / static_cast<double>(x);
    return std::pow(base, static_cast<double>(x));
}

extern "C" double Area(double a, double b) {
    return a * b;
}
