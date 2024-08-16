#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include <unordered_map>
#include <set>
#include <memory>

#include "order.hpp"
#include "price_level.hpp"

/**
 * @class OrderBook
 * Represents an order book for a single financial instrument.
 * 
 * The OrderBook class manages the bids and asks for a particular instrument,
 * handling order placement, cancellation, and matching.
 */
class OrderBook {
public:
    /**
     * Places a new order in the book or matches it against existing orders.
     * 
     * @param order A shared pointer to the Order to be placed.
     * @return true if the order was successfully placed or fully matched, false otherwise.
     */
    bool PlaceOrder(std::shared_ptr<Order> order);

    /**
     * Cancels an existing order in the book.
     * 
     * @param order_id The ID of the order to be cancelled.
     * @return true if the order was successfully cancelled, false if the order doesn't exist.
     */

    bool CancelOrder(OrderID order);

    /**
     * Checks if an incoming order can be fully filled based on current book state.
     * 
     * @param order A shared pointer to the Order to be checked.
     * @return true if the order can be fully filled, false otherwise.
     */
    bool CanFill(std::shared_ptr<Order> order);

    /**
     * Attempts to fill an incoming order against existing orders in the book.
     * 
     * @param order A shared pointer to the Order to be filled.
     */
    void Fill(std::shared_ptr<Order> order);
private:
    std::unordered_map<OrderPrice, PriceLevel> asks_; ///< Map of ask price levels.
    std::unordered_map<OrderPrice, PriceLevel> bids_; ///< Map of bid price levels.
    std::unordered_map<OrderID, std::tuple<OrderSide, OrderPrice>> orders_; ///< Map of all orders in the book.
    std::set<OrderPrice, std::less<OrderPrice>> best_asks_; ///< Sorted set of best ask prices.
    std::set<OrderPrice, std::greater<OrderPrice>> best_bids_; ///< Sorted set of best bid prices.
};

#endif