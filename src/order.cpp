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
    return quantity_ - filled_;
}

void Order::Fill(OrderQuantity amount) {
    if (amount > GetRemaining()) throw std::invalid_argument("Attempting to fill order more than capacity");
    filled_ += amount;
}

bool Order::IsFilled() {
    return GetRemaining() == 0;
}

Timestamp Order::GetCreatedAt() {
    return created_at_;
}

OrderID Order::GetID() {
    return id_;
}

OrderPrice Order::GetPrice() {
    return price_;
}

OrderQuantity Order::GetQuantity() {
    return quantity_;
}

OrderQuantity Order::GetFilled() {
    return filled_;
}

OrderSide Order::GetSide() {
    return side_;
}

OrderType Order::GetType() {
    return type_;
}

