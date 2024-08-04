#include "order.hpp"

Order::Order(OrderID order_id, std::string ticker, OrderPrice order_price, OrderQuantity order_quantity, OrderSide order_side, OrderType order_type)
    : created_at_{CurrentTime()}
    , id_{order_id}
    , ticker_{ticker}
    , price_{order_price}
    , quantity_{order_quantity}
    , filled_{0}
    , side_{order_side}
    , type_{order_type}
    , status_{OrderStatus::OPEN} {
    if (order_quantity == 0) throw std::invalid_argument("Attempting to create an order with no quantity");
}

OrderQuantity Order::GetRemaining() {
    return quantity_ - filled_;
}

void Order::Fill(OrderQuantity amount) {
    if (amount > GetRemaining()) throw std::invalid_argument("Attempting to fill order more than capacity");
    filled_ += amount;
    if (IsFilled()) SetStatus(OrderStatus::CLOSED);
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

std::string Order::GetTicker() {
    return ticker_;
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

OrderStatus Order::GetStatus() {
    return status_;
}

void Order::SetStatus(OrderStatus status) {
    if (status == OrderStatus::OPEN) throw std::invalid_argument("Cannot reopen an order");
    if (status == OrderStatus::CLOSED && status_ != OrderStatus::OPEN) throw std::invalid_argument("Cannot close an order that is not open");
    if (status == OrderStatus::CANCELLED && status_ != OrderStatus::OPEN) throw std::invalid_argument("Cannot cancel an order that is not open");
    status_ = status;
}