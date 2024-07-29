#include "price_level.hpp"

PriceLevel::PriceLevel(): total_quantity_{0} {}

void PriceLevel::Add(std::shared_ptr<Order> order) {
    if (order_locations_.count(order->GetID())) throw std::invalid_argument("Order with ID already exists in the level");

    orders_.push_back(order);
    order_locations_[order->GetID()] = std::prev(orders_.end());
    total_quantity_ += order->GetRemaining();
}

void PriceLevel::Remove(OrderID id) {
    if (!order_locations_.count(id)) throw std::invalid_argument("Order with ID does not exist in the level");

    total_quantity_ -= (*order_locations_[id])->GetRemaining();
    orders_.erase(order_locations_[id]);
    order_locations_.erase(id);
}

bool PriceLevel::IsEmpty() {
    return orders_.empty();
}

bool PriceLevel::CanFill(OrderQuantity amount) {
    return amount <= total_quantity_;
}

void PriceLevel::Fill(std::shared_ptr<Order> order) {
    while (!order->IsFilled() && !IsEmpty()) {
        std::shared_ptr<Order> top = orders_.front();
        OrderQuantity fill_amount = std::min(order->GetRemaining(), top->GetRemaining());
        top->Fill(fill_amount);
        order->Fill(fill_amount);
        total_quantity_ -= fill_amount;
        if (top->IsFilled()) Remove(top->GetID());
    }
}

Quantity PriceLevel::GetTotalQuantity() {
    return total_quantity_;
}