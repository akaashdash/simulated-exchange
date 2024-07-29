#ifndef UTILS_HPP
#define UTILS_HPP

#include <chrono>

using OrderID = uint64_t;
using Timestamp = uint64_t;
using OrderPrice = uint32_t;
using Price = uint64_t;
using OrderQuantity = uint32_t;
using Quantity = uint64_t;

inline Timestamp CurrentTime() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

#endif