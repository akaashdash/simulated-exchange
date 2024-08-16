#ifndef ORDER_STATUS_HPP
#define ORDER_STATUS_HPP

/**
 * @enum OrderStatus
 * Represents the possible statuses of an order in the trading system.
 */
enum OrderStatus {
    OPEN,  ///< The order is active and can be filled or cancelled.
    CLOSED, ///< The order has been completely filled.
    CANCELLED ///< The order has been cancelled and is no longer active.
};

#endif