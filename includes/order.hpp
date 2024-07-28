#ifndef ORDER_HPP
#define ORDER_HPP

#include <cstdint>

#include "order_side.hpp"
#include "order_type.hpp"
#include "utils.hpp"

struct Order {
    Order() = delete;
    Order(OrderID order_id, Price order_price, Quantity order_quantity, OrderSide order_side, OrderType order_type)
        : id(order_id)
        , price(order_price)
        , quantity(order_quantity)
        , side(order_side)
        , type(order_type)
    {
        timestamp = CurrentTime();
    }

    OrderID id;
    Timestamp timestamp;
    Price price;
    Quantity quantity;
    OrderSide side;
    OrderType type;
};

#endif