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
    Exchange(int port);
    ~Exchange();

    void Start();
    void Stop();
private:
    void HandleClient(int conn);

    const int port_;
    std::atomic<bool> running_;
    std::list<std::thread> clients_;
};

#endif