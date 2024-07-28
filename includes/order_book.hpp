#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include <unordered_map>
#include <list>
#include <set>

#include "order.hpp"

class OrderBook {
public:
    OrderBook();
    bool PlaceOrder(OrderID order_id, Price order_price, Quantity order_quantity, OrderSide order_side, OrderType order_type);
    bool CancelOrder(OrderID order);
private:
    std::unordered_map<Price, std::list<Order>> asks_;
    std::unordered_map<Price, std::list<Order>> bids_;
    std::unordered_map<OrderID, std::tuple<OrderSide, Price, std::list<Order>::iterator>> orders_;
    std::set<Price> best_asks_;
    std::set<Price> best_bids_;
};

#endif