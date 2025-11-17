#include <iostream>

int main() {
    int x;
    while (std::cin >> x) {
        if (x < 0) {
            return 0;
        }
        if (x <= 1) {
            continue;
        }
        bool prime = true;
        for (int d = 2; 1ll * d * d <= x; ++d) {
            if (x % d == 0) {
                std::cout << x << std::endl;
                prime = false;
                break;
            }
        }
        if (prime) {
            return 0;
        }
    }
    return 0;
}