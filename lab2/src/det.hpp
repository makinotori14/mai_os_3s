#pragma once
#include <vector>

int sign(size_t x);
std::vector<std::vector<int>> minor(const std::vector<std::vector<int>>& a, size_t ei, size_t ej);
int det(const std::vector<std::vector<int>>& a);