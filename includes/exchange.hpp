#ifndef EXCHANGE_HPP
#define EXCHANGE_HPP

#include <unordered_map>
#include <shared_mutex>

#include "utils.hpp"
#include "order.hpp"
#include "order_book.hpp"

#define BUFFER_SIZE 1024

class Exchange {
public:
    Exchange(std::string name, int port);
    ~Exchange();
    void Start();
    void Stop();
private:
    void HandleClient(int conn);
    mutable std::shared_mutex mutex_;
    const std::string name_;
    const int port_;
    std::unordered_map<std::string, int> clients_;
    std::unordered_map<std::string, std::unique_ptr<OrderBook>> order_books_;
    std::unordered_map<OrderID, std::shared_ptr<Order>> orders_;
};

#endif