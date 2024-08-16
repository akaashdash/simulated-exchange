#ifndef ORDER_HPP
#define ORDER_HPP

#include <cstdint>
#include <string>

#include "order_side.hpp"
#include "order_type.hpp"
#include "order_status.hpp"
#include "utils.hpp"

/**
 * @class Order
 * Represents an order in the trading system.
 *
 * This class encapsulates all the information and operations related to a single order.
 */
class Order {
public:
    /**
     * Deleted default constructor to prevent creation of invalid orders.
     */
    Order() = delete;

    /**
     * Constructs a new Order object.
     *
     * @param order_id Unique identifier for the order
     * @param ticker Stock ticker symbol
     * @param order_price Price of the order
     * @param order_quantity Quantity of the order
     * @param order_side Side of the order (BID or ASK)
     * @param order_type Type of the order (e.g., GOOD_TIL_CANCELED, FILL_OR_KILL)
     * @throws std::invalid_argument if order_quantity is 0
     */
    Order(OrderID order_id, std::string ticker, OrderPrice order_price, OrderQuantity order_quantity,
        OrderSide order_side, OrderType order_type);

    /**
     * Get the remaining unfilled quantity of the order.
     * 
     * @return Remaining quantity
     */
    OrderQuantity GetRemaining();

    /**
     * Fill a portion of the order.
     * 
     * @param amount Amount to fill
     * @throws std::invalid_argument if amount is greater than remaining quantity
     */
    void Fill(OrderQuantity amount);

    /**
     * Check if the order is completely filled.
     * 
     * @return true if filled, false otherwise
     */
    bool IsFilled();

    // Getters
    Timestamp GetCreatedAt();
    OrderID GetID();
    std::string GetTicker();
    OrderPrice GetPrice();
    OrderQuantity GetQuantity();
    OrderQuantity GetFilled();
    OrderSide GetSide();
    OrderType GetType();
    OrderStatus GetStatus();

    /**
     * Set the status of the order.
     * 
     * @param status New status
     * @throws std::invalid_argument for invalid state transitions
     */
    void SetStatus(OrderStatus status);
private:
    Timestamp created_at_; ///< Timestamp when the order was created.
    OrderID id_; ///< Unique identifier for the order.
    std::string ticker_; ///< Stock ticker symbol.
    OrderPrice price_; ///< Price of the order.
    OrderQuantity quantity_; ///< Total quantity of the order.
    OrderQuantity filled_; ///< Quantity that has been filled.
    OrderSide side_; ///< Side of the order.
    OrderType type_; ///< Type of the order.
    OrderStatus status_; ///< Current status of the order.
};

#endif