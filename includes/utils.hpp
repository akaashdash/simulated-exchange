#ifndef UTILS_HPP
#define UTILS_HPP

#include <chrono>

/**
 * @typedef OrderID
 * Unique identifier for orders.
 */
using OrderID = uint64_t;

/**
 * @typedef Timestamp
 * Timestamp in nanoseconds since epoch.
 */
using Timestamp = uint64_t;

/**
 * @typedef OrderPrice
 * Price of an order.
 */
using OrderPrice = uint32_t;

/**
 * @typedef Price
 * General price type (used for calculations).
 */
using Price = uint64_t;

/**
 * @typedef OrderQuantity
 * Quantity of an order.
 */
using OrderQuantity = uint32_t;

/**
 * @typedef Quantity
 * General quantity type (used for calculations).
 */
using Quantity = uint64_t;


/**
 * Get the current time in nanoseconds since epoch.
 * 
 * @return Current timestamp.
 */
inline Timestamp CurrentTime() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

#endif