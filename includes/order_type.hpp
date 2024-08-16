#ifndef ORDER_TYPE_HPP
#define ORDER_TYPE_HPP

/**
 * @enum OrderType
 * Represents the different types of orders that can be placed in the trading system.
 */
enum OrderType {
    GOOD_TIL_CANCELED, ///< Order remains active until explicitly canceled.
    FILL_OR_KILL, ///< Order must be filled immediately in its entirety or canceled.
    IMMEDIATE_OR_CANCEL ///< Order must be filled immediately, partially or fully, with any unfilled portion canceled.
};

#endif