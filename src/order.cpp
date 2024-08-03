#include "order.hpp"

Order::Order(OrderID order_id, OrderPrice order_price, OrderQuantity order_quantity, OrderSide order_side, OrderType order_type)
    : created_at_(CurrentTime())
    , id_(order_id)
    , price_(order_price)
    , quantity_(order_quantity)
    , filled_(0)
    , side_(order_side)
    , type_(order_type) {
    if (order_quantity == 0) throw std::invalid_argument("Attempting to create an order with no quantity");
}

OrderQuantity Order::GetRemaining() {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return quantity_ - filled_;
}

void Order::Fill(OrderQuantity amount) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (amount > GetRemaining()) throw std::invalid_argument("Attempting to fill order more than capacity");
    filled_ += amount;
}

bool Order::IsFilled() {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return GetRemaining() == 0;
}

Timestamp Order::GetCreatedAt() {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return created_at_;
}

OrderID Order::GetID() {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return id_;
}

OrderPrice Order::GetPrice() {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return price_;
}

OrderQuantity Order::GetQuantity() {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return quantity_;
}

OrderQuantity Order::GetFilled() {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return filled_;
}

OrderSide Order::GetSide() {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return side_;
}

OrderType Order::GetType() {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return type_;
}

