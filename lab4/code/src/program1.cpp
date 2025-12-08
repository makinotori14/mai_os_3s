#include <iostream>
#include <sstream>
#include <string>
#include "contracts.h"

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream iss(line);
        int cmd;
        if (!(iss >> cmd)) {
            continue;
        }
        if (cmd == 1) {
            int x;
            if (!(iss >> x)) {
                std::cout << "error" << std::endl;
                continue;
            }
            double value = E(x);
            std::cout.setf(std::ios::fixed);
            std::cout.precision(10);
            std::cout << value << std::endl;
        } else if (cmd == 2) {
            double a;
            double b;
            if (!(iss >> a >> b)) {
                std::cout << "error" << std::endl;
                continue;
            }
            double value = Area(a, b);
            std::cout.setf(std::ios::fixed);
            std::cout.precision(10);
            std::cout << value << std::endl;
        }
    }

    return 0;
}
