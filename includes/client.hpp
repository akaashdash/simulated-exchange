#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <unordered_set>
#include <optional>

#include "hffix.hpp"
#include "order_side.hpp"
#include "order_type.hpp"
#include "utils.hpp"
#include "order.hpp"

#define BUFFER_SIZE 1024

class Client {
public:
    Client();
    ~Client();

    void Start(std::string exchange_host, int exchange_port);
    void Stop();
    void Logon();
    bool PlaceOrder(std::string ticker, OrderSide side, OrderType type, OrderPrice price, OrderQuantity quantity);
    bool CancelOrder(OrderID id);
    std::optional<Order> GetOrderStatus(OrderID id);
private:
    int client_sock_;
    std::unordered_set<OrderID> orders_;
};

#endif