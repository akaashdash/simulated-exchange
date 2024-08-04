#include "order_book.hpp"

bool OrderBook::PlaceOrder(std::shared_ptr<Order> order) {
    // maybe return false instead?
    if (orders_.count(order->GetID())) throw std::invalid_argument("Order with ID already exists in the book");

    // Fail if not possible to fill FoK
    if (order->GetType() == OrderType::FILL_OR_KILL && !CanFill(order)) return false; 
    // Fill as much as we can
    Fill(order);
    // Kill FoK/IoC, don't add to book
    if (order->GetType() == OrderType::FILL_OR_KILL || order->GetType() == OrderType::IMMEDIATE_OR_CANCEL) return true;
    // Order is already filled, don't add to book
    if (order->IsFilled()) return true;

    // Add to book
    std::unordered_map<OrderPrice, PriceLevel>& book = (order->GetSide() == OrderSide::ASK) ? asks_ : bids_;
    book[order->GetPrice()].Add(order);
    orders_[order->GetID()] = {order->GetSide(), order->GetPrice()};
    
    // these are both currently O(logn) operations, need to optimize
    if (order->GetSide() == OrderSide::ASK) best_asks_.insert(order->GetPrice());
    else best_bids_.insert(order->GetPrice());
    return true;
}

bool OrderBook::CancelOrder(OrderID id) {
    // maybe return false instead?
    if (!orders_.count(id)) throw std::invalid_argument("Order with ID does not exist in the book");

    const auto& [side, price] = orders_[id];
    std::unordered_map<OrderPrice, PriceLevel>& book = (side == OrderSide::ASK) ? asks_ : bids_;
    book[price].Remove(id);
    if (book[price].IsEmpty()) {
        book.erase(price);
        // these are both currently O(logn) operations, need to optimize
        if (side == OrderSide::ASK) best_asks_.erase(price);
        else best_bids_.erase(price);
    }
    // currently setting order cancel status in exchange, maybe set here?
    orders_.erase(id);
    return true;
}

bool OrderBook::CanFill(std::shared_ptr<Order> order) {
    Quantity available = 0;
    if (order->GetSide() == OrderSide::ASK) {
        for (
            auto it = best_bids_.begin(); 
            it != best_bids_.end() && *it >= order->GetPrice(); 
            ++it
        ) {
            available += bids_[*it].GetTotalQuantity();
            if (available >= order->GetRemaining()) return true;
        }
    } else {
        for (
            auto it = best_asks_.begin(); 
            it != best_asks_.end() && *it <= order->GetPrice(); 
            ++it
        ) {
            available += asks_[*it].GetTotalQuantity();
            if (available >= order->GetRemaining()) return true;
        }
    }
    return available >= order->GetRemaining();
}

void OrderBook::Fill(std::shared_ptr<Order> order) {
    // Maybe combine with CanFill to avoid repeated code
    if (order->GetSide() == OrderSide::ASK) {
        auto it = best_bids_.begin(); 
        while (
            it != best_bids_.end() && 
            *it >= order->GetPrice() && 
            !order->IsFilled()
        ) {
            bids_[*it].Fill(order);
            if (bids_[*it].IsEmpty()) {
                bids_.erase(*it);
                // O(logn) operation, need to optimize
                it = best_bids_.erase(it);
            } else {
                ++it;
            }
        }
    } else {
        auto it = best_asks_.begin(); 
        while (
            it != best_asks_.end() && 
            *it <= order->GetPrice() && 
            !order->IsFilled()
        ) {
            asks_[*it].Fill(order);
            if (asks_[*it].IsEmpty()) {
                asks_.erase(*it);
                // O(logn) operation, need to optimize
                it = best_asks_.erase(it);
            } else {
                ++it;
            }
        }
    }
}