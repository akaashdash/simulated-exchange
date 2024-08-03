#ifndef ORDER_HPP
#define ORDER_HPP

#include <cstdint>
#include <shared_mutex>

#include "order_side.hpp"
#include "order_type.hpp"
#include "utils.hpp"

class Order {
public:
    Order() = delete;
    Order(OrderID order_id, OrderPrice order_price, OrderQuantity order_quantity, OrderSide order_side, OrderType order_type);
    OrderQuantity GetRemaining();
    void Fill(OrderQuantity amount);
    bool IsFilled();

    Timestamp GetCreatedAt();
    OrderID GetID();
    OrderPrice GetPrice();
    OrderQuantity GetQuantity();
    OrderQuantity GetFilled();
    OrderSide GetSide();
    OrderType GetType();
private:
    mutable std::shared_mutex mutex_;
    Timestamp created_at_;
    OrderID id_;
    OrderPrice price_;
    OrderQuantity quantity_;
    OrderQuantity filled_;
    OrderSide side_;
    OrderType type_;
};

#endif