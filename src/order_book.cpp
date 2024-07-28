#include "order_book.hpp"

bool OrderBook::PlaceOrder(OrderID id, Price price, Quantity quantity, OrderSide side, OrderType type) {
    if (orders_.count(id)) return false;
    if (type) throw std::invalid_argument("Market orders are not yet implemented");
    if (side) {
        asks_[price].emplace_back(id, price, quantity, side, type);
        orders_[id] = {side, price, std::prev(asks_[price].end())};
        best_asks_.insert(price);
    } else {
        bids_[price].emplace_back(id, price, quantity, side, type);
        orders_[id] = {side, price, std::prev(bids_[price].end())};
        best_bids_.insert(price);
    }
    return true;
}

bool OrderBook::CancelOrder(OrderID id) {
    if (!orders_.count(id)) return false;
    if (std::get<0>(orders_[id])) {
        asks_[std::get<1>(orders_[id])].erase(std::get<2>(orders_[id]));
        if (asks_[std::get<1>(orders_[id])].empty()) {
            asks_.erase(std::get<1>(orders_[id]));
            best_asks_.erase(std::get<1>(orders_[id]));
        }
    } else {
        bids_[std::get<1>(orders_[id])].erase(std::get<2>(orders_[id]));
        if (bids_[std::get<1>(orders_[id])].empty()) {
            bids_.erase(std::get<1>(orders_[id]));
            best_bids_.erase(std::get<1>(orders_[id]));
        }
    }
    orders_.erase(id);
    return true;
}