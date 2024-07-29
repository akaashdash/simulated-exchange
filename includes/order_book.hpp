#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include <unordered_map>
#include <list>
#include <set>
#include <any>
#include <shared_mutex>

#include "order.hpp"
#include "price_level.hpp"

class OrderBook {
public:
    bool PlaceOrder(std::shared_ptr<Order> order);
    bool CancelOrder(OrderID order);
    bool CanFill(std::shared_ptr<Order> order);
    void Fill(std::shared_ptr<Order> order);
private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<OrderPrice, PriceLevel> asks_;
    std::unordered_map<OrderPrice, PriceLevel> bids_;
    std::unordered_map<OrderID, std::tuple<OrderSide, OrderPrice>> orders_;
    std::set<OrderPrice, std::less<OrderPrice>> best_asks_;
    std::set<OrderPrice, std::greater<OrderPrice>> best_bids_;
};

#endif