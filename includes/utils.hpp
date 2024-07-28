#ifndef UTILS_HPP
#define UTILS_HPP

#include <chrono>

inline uint64_t CurrentTime() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

using OrderID = uint64_t;
using Timestamp = uint64_t;
using Price = uint32_t;
using Quantity = uint32_t;

#endif