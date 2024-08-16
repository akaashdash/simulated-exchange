#include "client.hpp"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdexcept>
#include <cstring>
    
Client::Client() : client_sock_{-1} {}

Client::~Client() {
    Stop();
}

void Client::Start(std::string exchange_host, int exchange_port) {
    // Create a socket
    client_sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock_ == -1) throw std::runtime_error("Socket creation failed");

    // Set up the server address structure
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(exchange_port);
    if (!inet_aton(exchange_host.c_str(), &server_addr.sin_addr)) {
        Stop();
        throw std::invalid_argument("Exchange host is invalid");
    }

    // Connect to the server
    if (connect(client_sock_, (struct sockaddr*) &server_addr, sizeof(server_addr))  == -1) {
        Stop();
        throw std::runtime_error("Failed to connect to exchange");
    }

    // Perform logon
    Logon();
}

void Client::Stop() {
    if (client_sock_ != -1) {
        close(client_sock_);
        client_sock_ = -1;
    }
}

void Client::Logon() {
    char message[BUFFER_SIZE];
    hffix::message_writer writer(message, message + BUFFER_SIZE);

    // Construct logon message
    writer.push_back_header("FIX.4.2");
    writer.push_back_string(hffix::tag::MsgType, "A");
    writer.push_back_string(hffix::tag::SenderCompID, "CLIENT");
    writer.push_back_string(hffix::tag::TargetCompID, "SERVER");
    writer.push_back_int(hffix::tag::EncryptMethod, 0);
    writer.push_back_trailer();

    // Send logon message
    if (send(client_sock_, message, writer.message_end() - message, 0) == -1) {
        Stop();
        throw std::runtime_error("Failed to send logon message");
    }

    // Receive logon response
    char response[BUFFER_SIZE] = {0};
    ssize_t len = recv(client_sock_, response, BUFFER_SIZE, 0);
    if (len <= 0) {
        Stop();
        throw std::runtime_error("Failed to receive logon response");
    }

    // Validate logon response
    hffix::message_reader reader(response, response + len);
    for (const auto& field : reader) {
        if (field.tag() == hffix::tag::MsgType && field.value() == "A") return;
    }

    // only reaches here if logon response is wrong
    Stop();
    throw std::runtime_error("Incorrect logon response received");
}

bool Client::PlaceOrder(std::string ticker, OrderSide side, OrderType type, OrderPrice price, OrderQuantity quantity) {
    char message[BUFFER_SIZE];

    // Construct new order message
    hffix::message_writer writer(message, message + BUFFER_SIZE);
    writer.push_back_header("FIX.4.2");
    writer.push_back_string(hffix::tag::MsgType, "D");
    writer.push_back_string(hffix::tag::SenderCompID, "CLIENT");
    writer.push_back_string(hffix::tag::TargetCompID, "SERVER");
    writer.push_back_string(hffix::tag::Symbol, ticker);
    writer.push_back_char(hffix::tag::Side, side == OrderSide::BID ? '1' : '2');

    char order_type;
    if (type == OrderType::FILL_OR_KILL) order_type = '3';
    else if (type == OrderType::GOOD_TIL_CANCELED) order_type = '1';
    else if (type == OrderType::IMMEDIATE_OR_CANCEL) order_type = '4';
    else return false; // can never reach here
    writer.push_back_char(hffix::tag::OrdType, order_type);

    writer.push_back_int(hffix::tag::Price, price);
    writer.push_back_int(hffix::tag::OrderQty, quantity);
    writer.push_back_trailer();

    // Send new order message
    if (send(client_sock_, message, writer.message_end() - message, 0) == -1) return false;

    // Receive order acknowledgment
    char response[BUFFER_SIZE] = {0};
    ssize_t len = recv(client_sock_, response, BUFFER_SIZE, 0);
    if (len <= 0) return false;

    // Validate order acknowledgment
    hffix::message_reader reader(response, response + len);
    OrderID id;
    for (const auto& field : reader) {
        if (field.tag() == hffix::tag::MsgType && field.value() != "8") return false;
        if (field.tag() == hffix::tag::SenderCompID && field.value() != "SERVER") return false;
        if (field.tag() == hffix::tag::TargetCompID && field.value() != "CLIENT") return false;
        if (field.tag() == hffix::tag::OrderID) id = field.value().as_int<OrderID>();
        if (field.tag() == hffix::tag::ExecType && field.value() != "0") return false;
        if (field.tag() == hffix::tag::OrdStatus && field.value() != "0") return false;
        if (field.tag() == hffix::tag::Symbol && field.value() != ticker) return false;
        if (field.tag() == hffix::tag::Side && field.value().as_char() != (side == OrderSide::BID ? '1' : '2')) return false;
        if (field.tag() == hffix::tag::OrderQty && field.value().as_int<OrderQuantity>() != quantity) return false;
        if (field.tag() == hffix::tag::Price && field.value().as_int<OrderPrice>() != price) return false;
    }

    orders_.insert(id);
    return true;
}


bool Client::CancelOrder(OrderID id) {
    if (!orders_.count(id)) return false;

    char message[BUFFER_SIZE];
    hffix::message_writer writer(message, message + BUFFER_SIZE);

    // Construct cancel order message
    writer.push_back_header("FIX.4.2");
    writer.push_back_string(hffix::tag::MsgType, "F");
    writer.push_back_string(hffix::tag::SenderCompID, "CLIENT");
    writer.push_back_string(hffix::tag::TargetCompID, "SERVER");
    writer.push_back_int(hffix::tag::OrderID, id);
    writer.push_back_trailer();

    // Send cancel order message
    if (send(client_sock_, message, writer.message_end() - message, 0) == -1) return false;

    // Receive cancel acknowledgment
    char response[BUFFER_SIZE] = {0};
    ssize_t len = recv(client_sock_, response, BUFFER_SIZE, 0);
    if (len <= 0) return false;

    // Validate cancel acknowledgment
    hffix::message_reader reader(response, response + len);
    for (const auto& field : reader) {
        if (field.tag() == hffix::tag::MsgType && field.value() != "8") return false;
        if (field.tag() == hffix::tag::SenderCompID && field.value() != "SERVER") return false;
        if (field.tag() == hffix::tag::TargetCompID && field.value() != "CLIENT") return false;
        if (field.tag() == hffix::tag::OrderID && field.value().as_int<OrderID>() != id) return false;
        if (field.tag() == hffix::tag::ExecType && field.value() != "4") return false;
        if (field.tag() == hffix::tag::OrdStatus && field.value() != "4") return false;
    }

    orders_.erase(id);
    return true;
}


std::optional<Order> Client::GetOrderStatus(OrderID id) {
    if (!orders_.count(id)) return std::nullopt;

    char message[BUFFER_SIZE];
    hffix::message_writer writer(message, message + BUFFER_SIZE);

    // Construct order status request message
    writer.push_back_header("FIX.4.2");
    writer.push_back_string(hffix::tag::MsgType, "H");
    writer.push_back_string(hffix::tag::SenderCompID, "CLIENT");
    writer.push_back_string(hffix::tag::TargetCompID, "SERVER");
    writer.push_back_int(hffix::tag::OrderID, id);
    writer.push_back_trailer();

    // Send order status request
    if (send(client_sock_, message, writer.message_end() - message, 0) == -1) return std::nullopt;

    // Receive order status
    char response[BUFFER_SIZE] = {0};
    ssize_t len = recv(client_sock_, response, BUFFER_SIZE, 0);
    if (len <= 0) return std::nullopt;

    // Parse order status
    hffix::message_reader reader(response, response + len);
    std::string ticker;
    OrderSide side;
    OrderType type;
    OrderPrice price;
    OrderQuantity quantity;
    OrderQuantity filled;
    OrderStatus status = OrderStatus::OPEN;
    
    for (const auto& field : reader) {
        if (field.tag() == hffix::tag::MsgType && field.value() != "8") return std::nullopt;
        if (field.tag() == hffix::tag::SenderCompID && field.value() != "SERVER") return std::nullopt;
        if (field.tag() == hffix::tag::TargetCompID && field.value() != "CLIENT") return std::nullopt;
        if (field.tag() == hffix::tag::OrderID && field.value().as_int<OrderID>() != id) return std::nullopt;
        if (field.tag() == hffix::tag::ExecType && field.value() != "I") return std::nullopt;
        if (field.tag() == hffix::tag::OrdStatus) {
            if (field.value().as_char() == '0') status = OrderStatus::OPEN;
            else if (field.value().as_char() == '1') status = OrderStatus::OPEN;
            else if (field.value().as_char() == '2') status = OrderStatus::CLOSED;
            else if (field.value().as_char() == '4') status = OrderStatus::CANCELLED;
            else return std::nullopt;
        }
        if (field.tag() == hffix::tag::Symbol) ticker = field.value().as_string();
        if (field.tag() == hffix::tag::Side) side = (field.value().as_char() == '1') ? OrderSide::BID : OrderSide::ASK;
        if (field.tag() == hffix::tag::OrdType) {
            if (field.value().as_char() == '1') type = OrderType::GOOD_TIL_CANCELED;
            else if (field.value().as_char() == '3') type = OrderType::FILL_OR_KILL;
            else if (field.value().as_char() == '4') type = OrderType::IMMEDIATE_OR_CANCEL;
            else return std::nullopt;
        }
        if (field.tag() == hffix::tag::OrderQty) quantity = field.value().as_int<OrderQuantity>();
        if (field.tag() == hffix::tag::CumQty) filled = field.value().as_int<OrderQuantity>();
        if (field.tag() == hffix::tag::Price) price = field.value().as_int<OrderPrice>();

    }
    
    Order order(id, ticker, price, quantity, side, type);
    order.Fill(filled);
    if (status != OrderStatus::OPEN) order.SetStatus(status);
    return order;
}