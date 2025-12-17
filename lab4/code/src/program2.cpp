#include <iostream>
#include <sstream>
#include <string>
#include <dlfcn.h>
#include "contracts.h"

using e_func_t = double (*)(int);
using area_func_t = double (*)(double, double);

struct Library {
    void* handle;
    e_func_t e;
    area_func_t area;
};

bool load_library(const char* path, Library& lib) {
    lib.handle = dlopen(path, RTLD_LAZY);
    if (!lib.handle) {
        std::cerr << dlerror() << std::endl;
        return false;
    }
    dlerror();
    lib.e = reinterpret_cast<e_func_t>(dlsym(lib.handle, "E"));
    const char* err1 = dlerror();
    if (err1) {
        std::cerr << err1 << std::endl;
        dlclose(lib.handle);
        lib.handle = nullptr;
        return false;
    }
    lib.area = reinterpret_cast<area_func_t>(dlsym(lib.handle, "Area"));
    const char* err2 = dlerror();
    if (err2) {
        std::cerr << err2 << std::endl;
        dlclose(lib.handle);
        lib.handle = nullptr;
        return false;
    }
    return true;
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    Library lib1{};
    Library lib2{};

    if (!load_library("./libimpl_first.so", lib1)) {
        return 1;
    }
    if (!load_library("./libimpl_second.so", lib2)) {
        dlclose(lib1.handle);
        return 1;
    }

    Library* current = &lib1;

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
        if (cmd == 0) {
            if (current == &lib1) {
                current = &lib2;
            } else {
                current = &lib1;
            }
            std::cout << "switched" << std::endl;
        } else if (cmd == 1) {
            int x;
            if (!(iss >> x)) {
                std::cout << "error" << std::endl;
                continue;
            }
            double value = current->e(x);
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
            double value = current->area(a, b);
            std::cout.setf(std::ios::fixed);
            std::cout.precision(10);
            std::cout << value << std::endl;
        }
    }

    dlclose(lib1.handle);
    dlclose(lib2.handle);

    return 0;
}
