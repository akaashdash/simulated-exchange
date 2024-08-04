#ifndef EXCHANGE_HPP
#define EXCHANGE_HPP

#include <unordered_map>
#include <shared_mutex>

#include "utils.hpp"
#include "order.hpp"
#include "order_book.hpp"
#include "hffix.hpp"

#define BUFFER_SIZE 1024

class Exchange {
public:
    Exchange();
    ~Exchange();

    void Start(int port);
    void Stop();
    void AddInstrument(std::string ticker);
    void RemoveInstrument(std::string ticker);
private:
    int HandleClient(int conn);
    bool ProcessLogon(hffix::message_reader& reader);
    void SendLogonResponse(int client_sock);
    void ProcessMessage(hffix::message_reader& reader, int client_sock);
    void ProcessNewOrder(hffix::message_reader& reader, int client_sock);
    void SendNewOrderAck(int client_sock, std::shared_ptr<Order>& order);
    void ProcessCancelOrder(hffix::message_reader& reader, int client_sock);
    void SendCancelOrderAck(int client_sock, OrderID order_id);
    void ProcessGetOrderStatus(hffix::message_reader& reader, int client_sock);
    void SendOrderStatus(int client_sock, std::shared_ptr<Order>& order);
    void SendRejection(int client_sock, std::string reason);

    std::atomic<bool> running_;
    std::atomic<OrderID> next_order_id_;
    mutable std::shared_mutex mutex_;
    std::unordered_map<OrderID, std::shared_ptr<Order>> orders_;
    std::unordered_map<std::string, std::unique_ptr<OrderBook>> order_books_;
};

#endif