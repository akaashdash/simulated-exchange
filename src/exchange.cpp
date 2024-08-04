#include "exchange.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <unistd.h>
#include <thread>

Exchange::Exchange() : running_{false}, next_order_id_{0} {}

Exchange::~Exchange() {
    Stop();
}

void Exchange::Start(int port) {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) throw std::runtime_error("Socket creation failed");

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_sock, (struct sockaddr*) &server_addr, sizeof(server_addr))  == -1) {
        close(server_sock);
        throw std::runtime_error("Socket binding failed");
    }

    if (listen(server_sock, 5)  == -1) {
        close(server_sock);
        throw std::runtime_error("Socket listening failed");
    }

    std::cout << "Exchange started on port " << port << std::endl;
    running_ = true;

    while (running_) {
        int client_sock = accept(server_sock, (struct sockaddr*)nullptr, nullptr);
        if (client_sock != -1) std::thread(&Exchange::HandleClient, this, client_sock).detach();
    }

    close(server_sock);
}

void Exchange::Stop() {
    running_ = false;
}

void Exchange::AddInstrument(std::string ticker) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (running_) throw std::runtime_error("Cannot add an instrument while the exchange is running");
    if (order_books_.count(ticker)) throw std::invalid_argument("Book with ticker already exists on exchange");
    order_books_.emplace(ticker, std::make_unique<OrderBook>());
}

void Exchange::RemoveInstrument(std::string ticker) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (running_) throw std::runtime_error("Cannot remove an instrument while the exchange is running");
    if (!order_books_.count(ticker)) throw std::invalid_argument("Book with ticker does not exist on exchange");
    order_books_.erase(ticker);
}

int Exchange::HandleClient(int client_sock) {
    char buffer[BUFFER_SIZE] = {0};

    ssize_t len = recv(client_sock, buffer, BUFFER_SIZE, 0);
    if (len <= 0) return close(client_sock);

    hffix::message_reader reader(buffer, buffer + len);
    // respond with error before closing?
    if (!ProcessLogon(reader)) return close(client_sock);
    SendLogonResponse(client_sock);

    while (running_) {
        memset(buffer, 0, BUFFER_SIZE);
        len = recv(client_sock, buffer, BUFFER_SIZE, 0);
        if (len <= 0) break;

        reader = hffix::message_reader(buffer, buffer + len);
        ProcessMessage(reader, client_sock);
    }

    return close(client_sock);
}

bool Exchange::ProcessLogon(hffix::message_reader& reader) {
    for (const auto& field : reader) {
        if (field.tag() == hffix::tag::MsgType && field.value() != "A") return false;
        if (field.tag() == hffix::tag::SenderCompID && field.value() != "CLIENT") return false;
        if (field.tag() == hffix::tag::TargetCompID && field.value() != "SERVER") return false;
        if (field.tag() == hffix::tag::EncryptMethod && field.value().as_int<int>() != 0) return false;
    }
    return true;
}

void Exchange::SendLogonResponse(int client_sock) {
    char response[BUFFER_SIZE];
    hffix::message_writer writer(response, response + BUFFER_SIZE);
    writer.push_back_header("FIX.4.2");
    writer.push_back_string(hffix::tag::MsgType, "A");
    writer.push_back_string(hffix::tag::SenderCompID, "SERVER");
    writer.push_back_string(hffix::tag::TargetCompID, "CLIENT");
    writer.push_back_int(hffix::tag::EncryptMethod, 0);
    writer.push_back_trailer();

    send(client_sock, response, writer.message_end() - response, 0);
}

void Exchange::ProcessMessage(hffix::message_reader& reader, int client_sock) {
    for (const auto& field : reader) {
        if (field.tag() == hffix::tag::MsgType) {
            if (field.value() == "D") ProcessNewOrder(reader, client_sock);
            else if (field.value() == "F") ProcessCancelOrder(reader, client_sock);
            else if (field.value() == "H") ProcessGetOrderStatus(reader, client_sock);
            return;
        }
    }
}

void Exchange::ProcessNewOrder(hffix::message_reader& reader, int client_sock) {
    std::string ticker;
    OrderSide side;
    OrderType type;
    OrderPrice price;
    OrderQuantity quantity;

    for (const auto& field : reader) {
        if (field.tag() == hffix::tag::Symbol) ticker = field.value().as_string();
        if (field.tag() == hffix::tag::Side) {
            if (field.value().as_char() == '1') side = OrderSide::BID;
            else if (field.value().as_char() == '2') side = OrderSide::ASK;
            else return SendRejection(client_sock, "Invalid order type");
        }
         if (field.tag() == hffix::tag::OrdType) {
            if (field.value().as_char() == '1') type = OrderType::GOOD_TIL_CANCELED;
            else if (field.value().as_char() == '3') type = OrderType::FILL_OR_KILL;
            else if (field.value().as_char() == '4') type = OrderType::IMMEDIATE_OR_CANCEL;
            else return SendRejection(client_sock, "Invalid order type");
        }
        if (field.tag() == hffix::tag::Price) price = field.value().as_int<OrderPrice>();
        if (field.tag() == hffix::tag::OrderQty) quantity = field.value().as_int<OrderQuantity>();
    }

    std::shared_lock<std::shared_mutex> read_lock(mutex_);
    bool exists = order_books_.count(ticker);
    read_lock.unlock();
    if (!exists) return SendRejection(client_sock, "Invalid symbol");

    std::shared_ptr<Order> order = std::make_shared<Order>(next_order_id_++, ticker, price, quantity, side, type);
    std::unique_lock<std::shared_mutex> lock(mutex_);
    orders_[order->GetID()] = order;
    bool success = order_books_[ticker]->PlaceOrder(order);
    lock.unlock();
    if (success) SendNewOrderAck(client_sock, order);
    else SendRejection(client_sock, "Order placement failed");
}


void Exchange::SendNewOrderAck(int client_sock, std::shared_ptr<Order>& order) {
    char response[BUFFER_SIZE];
    hffix::message_writer writer(response, response + BUFFER_SIZE);
    writer.push_back_header("FIX.4.2");
    writer.push_back_string(hffix::tag::MsgType, "8");
    writer.push_back_string(hffix::tag::SenderCompID, "SERVER");
    writer.push_back_string(hffix::tag::TargetCompID, "CLIENT");
    writer.push_back_int(hffix::tag::OrderID, order->GetID());
    writer.push_back_string(hffix::tag::ExecType, "0");
    writer.push_back_string(hffix::tag::OrdStatus, "0");
    writer.push_back_string(hffix::tag::Symbol, order->GetTicker());
    writer.push_back_char(hffix::tag::Side, order->GetSide() == OrderSide::BID ? '1' : '2');

    char order_type;
    if (order->GetType() == OrderType::FILL_OR_KILL) order_type = '3';
    else if (order->GetType()  == OrderType::GOOD_TIL_CANCELED) order_type = '1';
    else if (order->GetType()  == OrderType::IMMEDIATE_OR_CANCEL) order_type = '4';
    else return; // can never reach here
    writer.push_back_char(hffix::tag::OrdType, order_type);

    writer.push_back_char(hffix::tag::OrdType, order_type);
    writer.push_back_int(hffix::tag::OrderQty, order->GetQuantity());
    writer.push_back_int(hffix::tag::Price, order->GetPrice());
    writer.push_back_trailer();

    send(client_sock, response, writer.message_end() - response, 0);
}

void Exchange::ProcessCancelOrder(hffix::message_reader& reader, int client_sock) {
    OrderID id;

    for (const auto& field : reader) {
        if (field.tag() == hffix::tag::OrderID) id = field.value().as_int<OrderID>();
    }

    std::shared_lock<std::shared_mutex> read_lock(mutex_);
    bool exists = orders_.count(id);
    read_lock.unlock();
    if (!exists) return SendRejection(client_sock, "Invalid order ID");

    std::unique_lock<std::shared_mutex> lock(mutex_);
    // validate that order is cancelable right before cancelling
    bool success = order_books_[orders_[id]->GetTicker()]->CancelOrder(id);
    if (success) orders_[id]->SetStatus(OrderStatus::CANCELLED);
    lock.unlock();
    if (success) SendCancelOrderAck(client_sock, id);
    else SendRejection(client_sock, "Order cancellation failed");
}

void Exchange::SendCancelOrderAck(int client_sock, OrderID order_id) {
    char response[BUFFER_SIZE];
    hffix::message_writer writer(response, response + BUFFER_SIZE);
    writer.push_back_header("FIX.4.2");
    writer.push_back_string(hffix::tag::MsgType, "8");
    writer.push_back_string(hffix::tag::SenderCompID, "SERVER");
    writer.push_back_string(hffix::tag::TargetCompID, "CLIENT");
    writer.push_back_int(hffix::tag::OrderID, order_id);
    writer.push_back_string(hffix::tag::ExecType, "4");
    writer.push_back_string(hffix::tag::OrdStatus, "4");
    writer.push_back_trailer();

    send(client_sock, response, writer.message_end() - response, 0);
}

void Exchange::ProcessGetOrderStatus(hffix::message_reader& reader, int client_sock) {
    OrderID id;

    for (const auto& field : reader) {
        if (field.tag() == hffix::tag::OrderID) id = field.value().as_int<OrderID>();
    }

    std::shared_lock<std::shared_mutex> read_lock(mutex_);
    bool exists = orders_.count(id);
    std::shared_ptr<Order> order = orders_[id];
    read_lock.unlock();
    if (!exists) return SendRejection(client_sock, "Invalid order ID");
    
    SendOrderStatus(client_sock, order);
}

void Exchange::SendOrderStatus(int client_sock, std::shared_ptr<Order>& order) {
    char response[BUFFER_SIZE];
    hffix::message_writer writer(response, response + BUFFER_SIZE);
    // ideally no locking in send functions, even if not used for io
    std::shared_lock<std::shared_mutex> read_lock(mutex_);
    writer.push_back_header("FIX.4.2");
    writer.push_back_string(hffix::tag::MsgType, "8");
    writer.push_back_string(hffix::tag::SenderCompID, "SERVER");
    writer.push_back_string(hffix::tag::TargetCompID, "CLIENT");
    writer.push_back_int(hffix::tag::OrderID, order->GetID());
    writer.push_back_string(hffix::tag::ExecType, "I");

    std::string order_status;
    if (order->GetStatus() == OrderStatus::CLOSED) order_status = "2";
    else if (order->GetStatus() == OrderStatus::CANCELLED) order_status = "4";
    else order_status = order->IsFilled() ? "2" : (order->GetFilled() == 0 ? "0" : "1");
    writer.push_back_string(hffix::tag::OrdStatus, order_status);

    writer.push_back_string(hffix::tag::Symbol, order->GetTicker());
    writer.push_back_char(hffix::tag::Side, order->GetSide() == OrderSide::BID ? '1' : '2');

    char order_type;
    if (order->GetType() == OrderType::FILL_OR_KILL) order_type = '3';
    else if (order->GetType()  == OrderType::GOOD_TIL_CANCELED) order_type = '1';
    else if (order->GetType()  == OrderType::IMMEDIATE_OR_CANCEL) order_type = '4';
    else return; // can never reach here
    writer.push_back_char(hffix::tag::OrdType, order_type);
    
    writer.push_back_int(hffix::tag::OrderQty, order->GetQuantity());
    writer.push_back_int(hffix::tag::CumQty, order->GetFilled());
    writer.push_back_int(hffix::tag::LeavesQty, order->GetRemaining());
    writer.push_back_int(hffix::tag::Price, order->GetPrice());
    writer.push_back_trailer();
    read_lock.unlock();

    send(client_sock, response, writer.message_end() - response, 0);
}


void Exchange::SendRejection(int client_sock, std::string reason) {
    char response[BUFFER_SIZE];
    hffix::message_writer writer(response, response + BUFFER_SIZE);
    writer.push_back_header("FIX.4.2");
    writer.push_back_string(hffix::tag::MsgType, "3");
    writer.push_back_string(hffix::tag::SenderCompID, "SERVER");
    writer.push_back_string(hffix::tag::TargetCompID, "CLIENT");
    writer.push_back_string(hffix::tag::Text, reason);
    writer.push_back_trailer();

    send(client_sock, response, writer.message_end() - response, 0);
}