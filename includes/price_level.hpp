#ifndef PRICE_LEVEL_HPP
#define PRICE_LEVEL_HPP

#include <list>

#include "order.hpp"

class PriceLevel {
public:
    void Add(std::shared_ptr<Order> order);
    void Remove(OrderID id);
    bool IsEmpty();
    bool CanFill(OrderQuantity amount);
    void Fill(std::shared_ptr<Order> order);

    Quantity GetTotalQuantity();
private:
    std::list<std::shared_ptr<Order>> orders_;
    std::unordered_map<OrderID, std::list<std::shared_ptr<Order>>::iterator> order_locations_;
    Quantity total_quantity_;
};

#endif