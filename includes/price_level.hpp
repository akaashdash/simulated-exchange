#ifndef PRICE_LEVEL_HPP
#define PRICE_LEVEL_HPP

#include <list>
#include <unordered_map>
#include <memory>
#include <stdexcept>

#include "order.hpp"
#include "utils.hpp"

/**
 * @class PriceLevel
 * Represents a single price level in an order book.
 *
 * This class manages orders at a specific price point, providing
 * methods for adding, removing, and filling orders.
 */
class PriceLevel {
public:
    /**
     * Construct a new PriceLevel object.
     */
    PriceLevel();

    /**
     * Add an order to this price level.
     * 
     * @param order The order to be added.
     * @throw std::invalid_argument if an order with the same ID already exists.
     */
    void Add(std::shared_ptr<Order> order);

    /**
     * Remove an order from this price level.
     * 
     * @param id The ID of the order to be removed.
     * @throw std::invalid_argument if the order does not exist.
     */
    void Remove(OrderID id);

    /**
     * Check if this price level has no orders.
     * 
     * @return true if there are no orders, false otherwise.
     */
    bool IsEmpty();

    /**
     * Check if this price level can fill a given quantity.
     * 
     * @param amount The quantity to check for filling.
     * @return true if the level can fill the amount, false otherwise.
     */
    bool CanFill(OrderQuantity amount);

    /**
     * Fill an incoming order with orders from this price level.
     * 
     * @param order The incoming order to be filled.
     */
    void Fill(std::shared_ptr<Order> order);

    /**
     * Get the total quantity of all orders at this price level.
     * 
     * @return The total quantity.
     */
    Quantity GetTotalQuantity();
private:
    std::list<std::shared_ptr<Order>> orders_; ///< List of orders at this price level, maintained in FIFO order.
    std::unordered_map<OrderID, std::list<std::shared_ptr<Order>>::iterator> order_locations_; ///< Map for quick lookup of order locations in the orders list.
    Quantity total_quantity_; ///< Running sum of the total quantity of all orders at this price level.
};

#endif