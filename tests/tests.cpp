#include "catch.hpp"

#include "order_book.hpp"
#include "order.hpp"
#include "exchange.hpp"
#include "client.hpp"

#include <memory>
#include <chrono>
#include <thread>
#include <iostream>
#include <limits>
#include <cmath>
#include <deque>
#include <future>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <map>
#include <unordered_map>

///
/// Order tests
///

TEST_CASE("Order creation and basic properties", "[Order]") {
    OrderID id = 1;
    std::string ticker = "AAPL";
    OrderPrice price = 15000; // $150.00
    OrderQuantity quantity = 100;
    OrderSide side = OrderSide::BID;
    OrderType type = OrderType::GOOD_TIL_CANCELED;

    Order order(id, ticker, price, quantity, side, type);

    SECTION("Check initial state") {
        REQUIRE(order.GetID() == id);
        REQUIRE(order.GetTicker() == ticker);
        REQUIRE(order.GetPrice() == price);
        REQUIRE(order.GetQuantity() == quantity);
        REQUIRE(order.GetFilled() == 0);
        REQUIRE(order.GetRemaining() == quantity);
        REQUIRE(order.GetSide() == side);
        REQUIRE(order.GetType() == type);
        REQUIRE(order.GetStatus() == OrderStatus::OPEN);
        REQUIRE_FALSE(order.IsFilled());
    }

    SECTION("Check created timestamp") {
        Timestamp now = CurrentTime();
        REQUIRE(order.GetCreatedAt() <= now);
        REQUIRE(order.GetCreatedAt() > now - 1000000000); // Within 1 second
    }
}

TEST_CASE("Order filling", "[Order]") {
    Order order(1, "AAPL", 15000, 100, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);

    SECTION("Partial fill") {
        order.Fill(50);
        REQUIRE(order.GetFilled() == 50);
        REQUIRE(order.GetRemaining() == 50);
        REQUIRE(order.GetStatus() == OrderStatus::OPEN);
        REQUIRE_FALSE(order.IsFilled());
    }

    SECTION("Complete fill") {
        order.Fill(100);
        REQUIRE(order.GetFilled() == 100);
        REQUIRE(order.GetRemaining() == 0);
        REQUIRE(order.GetStatus() == OrderStatus::CLOSED);
        REQUIRE(order.IsFilled());
    }

    SECTION("Multiple partial fills") {
        order.Fill(30);
        order.Fill(40);
        order.Fill(30);
        REQUIRE(order.GetFilled() == 100);
        REQUIRE(order.GetRemaining() == 0);
        REQUIRE(order.GetStatus() == OrderStatus::CLOSED);
        REQUIRE(order.IsFilled());
    }

    SECTION("Overfill attempt") {
        REQUIRE_THROWS_AS(order.Fill(101), std::invalid_argument);
        REQUIRE(order.GetFilled() == 0);
        REQUIRE(order.GetRemaining() == 100);
        REQUIRE(order.GetStatus() == OrderStatus::OPEN);
    }

    SECTION("Fill after complete") {
        order.Fill(100);
        REQUIRE_THROWS_AS(order.Fill(1), std::invalid_argument);
        REQUIRE(order.GetFilled() == 100);
        REQUIRE(order.GetStatus() == OrderStatus::CLOSED);
    }
}

TEST_CASE("Order status changes", "[Order]") {
    Order order(1, "AAPL", 15000, 100, OrderSide::ASK, OrderType::FILL_OR_KILL);

    SECTION("Cancel open order") {
        order.SetStatus(OrderStatus::CANCELLED);
        REQUIRE(order.GetStatus() == OrderStatus::CANCELLED);
    }

    SECTION("Close open order") {
        order.SetStatus(OrderStatus::CLOSED);
        REQUIRE(order.GetStatus() == OrderStatus::CLOSED);
    }

    SECTION("Cannot reopen order") {
        order.SetStatus(OrderStatus::CANCELLED);
        REQUIRE_THROWS_AS(order.SetStatus(OrderStatus::OPEN), std::invalid_argument);
    }

    SECTION("Cannot close cancelled order") {
        order.SetStatus(OrderStatus::CANCELLED);
        REQUIRE_THROWS_AS(order.SetStatus(OrderStatus::CLOSED), std::invalid_argument);
    }

    SECTION("Cannot cancel closed order") {
        order.SetStatus(OrderStatus::CLOSED);
        REQUIRE_THROWS_AS(order.SetStatus(OrderStatus::CANCELLED), std::invalid_argument);
    }
}

TEST_CASE("Order edge cases", "[Order]") {
    SECTION("Create order with zero quantity") {
        REQUIRE_THROWS_AS(Order(1, "AAPL", 15000, 0, OrderSide::BID, OrderType::GOOD_TIL_CANCELED), std::invalid_argument);
    }

    SECTION("Create order with maximum quantity") {
        Order order(1, "AAPL", 15000, std::numeric_limits<OrderQuantity>::max(), OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(order.GetQuantity() == std::numeric_limits<OrderQuantity>::max());
    }

    SECTION("Create order with maximum price") {
        Order order(1, "AAPL", std::numeric_limits<OrderPrice>::max(), 100, OrderSide::ASK, OrderType::IMMEDIATE_OR_CANCEL);
        REQUIRE(order.GetPrice() == std::numeric_limits<OrderPrice>::max());
    }

    SECTION("Fill with zero quantity") {
        Order order(1, "AAPL", 15000, 100, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        REQUIRE_NOTHROW(order.Fill(0));
        REQUIRE(order.GetFilled() == 0);
        REQUIRE(order.GetStatus() == OrderStatus::OPEN);
    }
}

TEST_CASE("Order type and side combinations", "[Order]") {
    SECTION("BID GOOD_TIL_CANCELED") {
        Order order(1, "AAPL", 15000, 100, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(order.GetSide() == OrderSide::BID);
        REQUIRE(order.GetType() == OrderType::GOOD_TIL_CANCELED);
    }

    SECTION("ASK FILL_OR_KILL") {
        Order order(2, "GOOGL", 250000, 50, OrderSide::ASK, OrderType::FILL_OR_KILL);
        REQUIRE(order.GetSide() == OrderSide::ASK);
        REQUIRE(order.GetType() == OrderType::FILL_OR_KILL);
    }

    SECTION("BID IMMEDIATE_OR_CANCEL") {
        Order order(3, "MSFT", 30000, 75, OrderSide::BID, OrderType::IMMEDIATE_OR_CANCEL);
        REQUIRE(order.GetSide() == OrderSide::BID);
        REQUIRE(order.GetType() == OrderType::IMMEDIATE_OR_CANCEL);
    }
}

///
/// PriceLevel tests
///

// Helper function to create a shared_ptr<Order> for testing
std::shared_ptr<Order> createOrder(OrderID id, OrderPrice price, OrderQuantity quantity, OrderSide side) {
    return std::make_shared<Order>(id, "TEST", price, quantity, side, OrderType::GOOD_TIL_CANCELED);
}

TEST_CASE("PriceLevel basic operations", "[PriceLevel]") {
    PriceLevel level;

    SECTION("Initial state") {
        REQUIRE(level.IsEmpty());
        REQUIRE(level.GetTotalQuantity() == 0);
    }

    SECTION("Add single order") {
        auto order = createOrder(1, 10000, 100, OrderSide::BID);
        level.Add(order);

        REQUIRE_FALSE(level.IsEmpty());
        REQUIRE(level.GetTotalQuantity() == 100);
    }

    SECTION("Add multiple orders") {
        level.Add(createOrder(1, 10000, 100, OrderSide::BID));
        level.Add(createOrder(2, 10000, 200, OrderSide::BID));
        level.Add(createOrder(3, 10000, 300, OrderSide::BID));

        REQUIRE_FALSE(level.IsEmpty());
        REQUIRE(level.GetTotalQuantity() == 600);
    }

    SECTION("Remove order") {
        level.Add(createOrder(1, 10000, 100, OrderSide::BID));
        level.Add(createOrder(2, 10000, 200, OrderSide::BID));

        level.Remove(1);

        REQUIRE_FALSE(level.IsEmpty());
        REQUIRE(level.GetTotalQuantity() == 200);

        level.Remove(2);

        REQUIRE(level.IsEmpty());
        REQUIRE(level.GetTotalQuantity() == 0);
    }
}

TEST_CASE("PriceLevel fill operations", "[PriceLevel]") {
    PriceLevel level;
    level.Add(createOrder(1, 10000, 100, OrderSide::BID));
    level.Add(createOrder(2, 10000, 200, OrderSide::BID));
    level.Add(createOrder(3, 10000, 300, OrderSide::BID));

    SECTION("Can fill within total quantity") {
        REQUIRE(level.CanFill(300));
        REQUIRE(level.CanFill(600));
        REQUIRE_FALSE(level.CanFill(601));
    }

    SECTION("Partial fill") {
        auto order = createOrder(4, 10000, 250, OrderSide::ASK);
        level.Fill(order);

        REQUIRE(level.GetTotalQuantity() == 350);
        REQUIRE(order->GetFilled() == 250);
        REQUIRE_FALSE(level.IsEmpty());
    }

    SECTION("Complete fill") {
        auto order = createOrder(4, 10000, 600, OrderSide::ASK);
        level.Fill(order);

        REQUIRE(level.GetTotalQuantity() == 0);
        REQUIRE(order->GetFilled() == 600);
        REQUIRE(level.IsEmpty());
    }

    SECTION("Overfill attempt") {
        auto order = createOrder(4, 10000, 700, OrderSide::ASK);
        level.Fill(order);

        REQUIRE(level.GetTotalQuantity() == 0);
        REQUIRE(order->GetFilled() == 600);
        REQUIRE(level.IsEmpty());
    }
}

TEST_CASE("PriceLevel edge cases", "[PriceLevel]") {
    PriceLevel level;

    SECTION("Add order with maximum quantity") {
        auto order = createOrder(1, 10000, std::numeric_limits<OrderQuantity>::max(), OrderSide::BID);
        level.Add(order);
        REQUIRE(level.GetTotalQuantity() == std::numeric_limits<OrderQuantity>::max());
    }

    SECTION("Remove non-existent order") {
        REQUIRE_THROWS_AS(level.Remove(1), std::invalid_argument);
    }

    SECTION("Add duplicate order ID") {
        level.Add(createOrder(1, 10000, 100, OrderSide::BID));
        REQUIRE_THROWS_AS(level.Add(createOrder(1, 10000, 200, OrderSide::BID)), std::invalid_argument);
    }
}

TEST_CASE("PriceLevel order precedence", "[PriceLevel]") {
    PriceLevel level;
    level.Add(createOrder(1, 10000, 100, OrderSide::BID));
    level.Add(createOrder(2, 10000, 200, OrderSide::BID));
    level.Add(createOrder(3, 10000, 300, OrderSide::BID));

    SECTION("Fill respects order precedence") {
        auto order = createOrder(4, 10000, 350, OrderSide::ASK);
        level.Fill(order);

        // First two orders should be completely filled, third one partially
        REQUIRE_THROWS_AS(level.Remove(1), std::invalid_argument);
        REQUIRE_THROWS_AS(level.Remove(2), std::invalid_argument);
        REQUIRE(level.GetTotalQuantity() == 250);
        REQUIRE_NOTHROW(level.Remove(3));
        REQUIRE(level.GetTotalQuantity() == 0);
    }
}

///
/// OrderBook tests
///

// Helper function to create a shared_ptr<Order> for testing
std::shared_ptr<Order> createOrder(OrderID id, std::string ticker, OrderPrice price, OrderQuantity quantity, OrderSide side, OrderType type) {
    return std::make_shared<Order>(id, ticker, price, quantity, side, type);
}

TEST_CASE("OrderBook basic operations", "[OrderBook]") {
    OrderBook book;

    SECTION("Place single order") {
        auto order = createOrder(1, "AAPL", 15000, 100, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(book.PlaceOrder(order));
    }

    SECTION("Place and cancel order") {
        auto order = createOrder(1, "AAPL", 15000, 100, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(book.PlaceOrder(order));
        REQUIRE(book.CancelOrder(1));
    }

    SECTION("Cancel non-existent order") {
        REQUIRE_THROWS_AS(book.CancelOrder(999), std::invalid_argument);
    }
}

TEST_CASE("OrderBook matching", "[OrderBook]") {
    OrderBook book;

    SECTION("Match single order") {
        auto bid = createOrder(1, "AAPL", 15000, 100, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(book.PlaceOrder(bid));

        auto ask = createOrder(2, "AAPL", 15000, 100, OrderSide::ASK, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(book.PlaceOrder(ask));

        REQUIRE(bid->IsFilled());
        REQUIRE(ask->IsFilled());
    }

    SECTION("Partial match") {
        auto bid = createOrder(1, "AAPL", 15000, 100, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(book.PlaceOrder(bid));

        auto ask = createOrder(2, "AAPL", 15000, 50, OrderSide::ASK, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(book.PlaceOrder(ask));

        REQUIRE(bid->GetFilled() == 50);
        REQUIRE(ask->IsFilled());
    }

    SECTION("No match due to price") {
        auto bid = createOrder(1, "AAPL", 15000, 100, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(book.PlaceOrder(bid));

        auto ask = createOrder(2, "AAPL", 15100, 100, OrderSide::ASK, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(book.PlaceOrder(ask));

        REQUIRE(bid->GetFilled() == 0);
        REQUIRE(ask->GetFilled() == 0);
    }
}

TEST_CASE("OrderBook order types", "[OrderBook]") {
    OrderBook book;

    SECTION("Fill or Kill - fully filled") {
        auto bid = createOrder(1, "AAPL", 15000, 100, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(book.PlaceOrder(bid));

        auto fok = createOrder(2, "AAPL", 15000, 100, OrderSide::ASK, OrderType::FILL_OR_KILL);
        REQUIRE(book.PlaceOrder(fok));

        REQUIRE(fok->IsFilled());
        REQUIRE(bid->IsFilled());
    }

    SECTION("Fill or Kill - not filled") {
        auto bid = createOrder(1, "AAPL", 15000, 50, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(book.PlaceOrder(bid));

        auto fok = createOrder(2, "AAPL", 15000, 100, OrderSide::ASK, OrderType::FILL_OR_KILL);
        REQUIRE_FALSE(book.PlaceOrder(fok));

        REQUIRE(fok->GetFilled() == 0);
        REQUIRE(bid->GetFilled() == 0);
    }

    SECTION("Immediate or Cancel - partial fill") {
        auto bid = createOrder(1, "AAPL", 15000, 50, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(book.PlaceOrder(bid));

        auto ioc = createOrder(2, "AAPL", 15000, 100, OrderSide::ASK, OrderType::IMMEDIATE_OR_CANCEL);
        REQUIRE(book.PlaceOrder(ioc));

        REQUIRE(ioc->GetFilled() == 50);
        REQUIRE(bid->IsFilled());
    }
}

TEST_CASE("OrderBook edge cases", "[OrderBook]") {
    OrderBook book;

    SECTION("Place order with duplicate ID") {
        auto order1 = createOrder(1, "AAPL", 15000, 100, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(book.PlaceOrder(order1));

        auto order2 = createOrder(1, "AAPL", 15100, 100, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);

        REQUIRE_THROWS_AS(book.PlaceOrder(order2), std::invalid_argument);
    }

    SECTION("Cancel already filled order") {
        auto bid = createOrder(1, "AAPL", 15000, 100, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(book.PlaceOrder(bid));

        auto ask = createOrder(2, "AAPL", 15000, 100, OrderSide::ASK, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(book.PlaceOrder(ask));

        REQUIRE_THROWS_AS(book.CancelOrder(1), std::invalid_argument);
        REQUIRE_THROWS_AS(book.CancelOrder(2), std::invalid_argument);
    }

    SECTION("Match with maximum price difference") {
        auto bid = createOrder(1, "AAPL", std::numeric_limits<OrderPrice>::max(), 100, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(book.PlaceOrder(bid));

        auto ask = createOrder(2, "AAPL", 1, 100, OrderSide::ASK, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(book.PlaceOrder(ask));

        REQUIRE(bid->IsFilled());
        REQUIRE(ask->IsFilled());
    }
}

TEST_CASE("OrderBook complex scenarios", "[OrderBook]") {
    OrderBook book;

    SECTION("Multiple partial fills") {
        auto bid1 = createOrder(1, "AAPL", 15000, 100, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        auto bid2 = createOrder(2, "AAPL", 15000, 50, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(book.PlaceOrder(bid1));
        REQUIRE(book.PlaceOrder(bid2));

        auto ask1 = createOrder(3, "AAPL", 15000, 75, OrderSide::ASK, OrderType::GOOD_TIL_CANCELED);
        auto ask2 = createOrder(4, "AAPL", 15000, 100, OrderSide::ASK, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(book.PlaceOrder(ask1));
        REQUIRE(book.PlaceOrder(ask2));

        REQUIRE(bid1->IsFilled());
        REQUIRE(bid2->IsFilled());
        REQUIRE(ask1->IsFilled());
        REQUIRE(ask2->GetFilled() == 75);
    }

    SECTION("Price priority") {
        auto bid1 = createOrder(1, "AAPL", 15000, 100, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        auto bid2 = createOrder(2, "AAPL", 15100, 100, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(book.PlaceOrder(bid1));
        REQUIRE(book.PlaceOrder(bid2));

        auto ask = createOrder(3, "AAPL", 15000, 100, OrderSide::ASK, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(book.PlaceOrder(ask));

        REQUIRE(bid2->IsFilled());
        REQUIRE(bid1->GetFilled() == 0);
        REQUIRE(ask->IsFilled());
    }

    SECTION("Time priority") {
        auto bid1 = createOrder(1, "AAPL", 15000, 100, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        auto bid2 = createOrder(2, "AAPL", 15000, 100, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(book.PlaceOrder(bid1));
        REQUIRE(book.PlaceOrder(bid2));

        auto ask = createOrder(3, "AAPL", 15000, 100, OrderSide::ASK, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(book.PlaceOrder(ask));

        REQUIRE(bid1->IsFilled());
        REQUIRE(bid2->GetFilled() == 0);
        REQUIRE(ask->IsFilled());
    }
}

TEST_CASE("OrderBook stress test", "[OrderBook]") {
    OrderBook book;
    const int NUM_ORDERS = 10000;

    SECTION("Rapid order placement and cancellation") {
        std::vector<std::shared_ptr<Order>> orders;
        for (int i = 0; i < NUM_ORDERS; ++i) {
            auto order = createOrder(i, "AAPL", 15000 + i % 100, 100, i % 2 == 0 ? OrderSide::BID : OrderSide::ASK, OrderType::GOOD_TIL_CANCELED);
            orders.push_back(order);
            REQUIRE(book.PlaceOrder(order));
        }

        for (int i = 0; i < NUM_ORDERS; i += 2) {
            REQUIRE(book.CancelOrder(i));
        }

        int filled_count = 0;
        for (const auto& order : orders) {
            if (order->IsFilled()) {
                ++filled_count;
            }
        }

        REQUIRE(filled_count > 0);
    }
}

///
/// Exchange tests
///

// Helper function to parse FIX messages
bool parseFixMessage(const std::string& message, const std::string& expectedMsgType, std::map<int, std::string>& fields) {
    hffix::message_reader reader(message.data(), message.data() + message.size());
    if (!reader.is_valid()) return false;

    auto begin = reader.begin();
    auto end = reader.end();

    // Check message type
    auto msgTypeField = std::find_if(begin, end, [](const auto& field) {
        return field.tag() == hffix::tag::MsgType;
    });
    if (msgTypeField == end || msgTypeField->value() != expectedMsgType) return false;

    // Extract other fields
    for (auto it = begin; it != end; ++it) {
        fields[it->tag()] = it->value().as_string();
    }

    return true;
}

// Helper function to create a client and connect to the exchange
class TestClient {
public:
    TestClient(const std::string& host, int port) : host_(host), port_(port) {}

    bool Connect() {
        client_socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (client_socket_ == -1) return false;

        sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port_);
        if (inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr) <= 0) return false;

        return connect(client_socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) != -1;
    }

    bool SendMessage(const std::string& message) {
        return send(client_socket_, message.c_str(), message.length(), 0) != -1;
    }

    std::string ReceiveMessage() {
        char buffer[1024] = {0};
        ssize_t valread = read(client_socket_, buffer, 1024);
        if (valread > 0) {
            return std::string(buffer, valread);
        }
        return "";
    }

    void Close() {
        close(client_socket_);
    }

private:
    std::string host_;
    int port_;
    int client_socket_;
};

// Helper function to create FIX messages
std::string createFixMessage(const std::string& msgType, const std::vector<std::pair<int, std::string>>& fields) {
    char buffer[1024];
    hffix::message_writer writer(buffer, buffer + 1024);
    writer.push_back_header("FIX.4.2");
    writer.push_back_string(hffix::tag::MsgType, msgType);
    for (const auto& field : fields) {
        writer.push_back_string(field.first, field.second);
    }
    writer.push_back_trailer();
    return std::string(buffer, writer.message_end());
}

TEST_CASE("Exchange operations", "[Exchange]") {
    Exchange exchange;

    SECTION("Start and stop exchange") {
        std::future<void> exchange_thread = std::async(std::launch::async, [&exchange]() {
            exchange.Start(8080);
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        exchange.Stop();
        exchange_thread.wait();
    }

    SECTION("Add and remove instruments") {
        REQUIRE_NOTHROW(exchange.AddInstrument("AAPL"));
        REQUIRE_NOTHROW(exchange.AddInstrument("GOOGL"));
        REQUIRE_THROWS_AS(exchange.AddInstrument("AAPL"), std::invalid_argument);
        REQUIRE_NOTHROW(exchange.RemoveInstrument("AAPL"));
        REQUIRE_THROWS_AS(exchange.RemoveInstrument("MSFT"), std::invalid_argument);
    }
}

TEST_CASE("Exchange client interactions", "[Exchange]") {
    Exchange exchange;
    exchange.AddInstrument("AAPL");

    std::future<void> exchange_thread = std::async(std::launch::async, [&exchange]() {
        exchange.Start(8080);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    SECTION("Client logon and new order") {
        TestClient client("127.0.0.1", 8080);
        REQUIRE(client.Connect());

        // Send logon message
        std::string logon_msg = createFixMessage("A", {
            {hffix::tag::SenderCompID, "CLIENT"},
            {hffix::tag::TargetCompID, "SERVER"},
            {hffix::tag::EncryptMethod, "0"}
        });
        REQUIRE(client.SendMessage(logon_msg));

        // Receive logon response
        std::string logon_response = client.ReceiveMessage();
        REQUIRE(!logon_response.empty());
        std::map<int, std::string> logon_fields;
        REQUIRE(parseFixMessage(logon_response, "A", logon_fields));

        // Send new order message
        std::string new_order_msg = createFixMessage("D", {
            {hffix::tag::Symbol, "AAPL"},
            {hffix::tag::Side, "1"},
            {hffix::tag::OrdType, "1"},
            {hffix::tag::Price, "15000"},
            {hffix::tag::OrderQty, "100"}
        });
        REQUIRE(client.SendMessage(new_order_msg));

        // Receive new order acknowledgement
        std::string order_response = client.ReceiveMessage();
        REQUIRE(!order_response.empty());
        std::map<int, std::string> order_fields;
        REQUIRE(parseFixMessage(order_response, "8", order_fields));
        REQUIRE(order_fields[hffix::tag::OrdStatus] == "0");

        client.Close();
    }

    exchange.Stop();
    exchange_thread.wait();
}

TEST_CASE("Exchange error handling", "[Exchange]") {
    Exchange exchange;
    exchange.AddInstrument("AAPL");

    std::future<void> exchange_thread = std::async(std::launch::async, [&exchange]() {
        exchange.Start(8080);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    SECTION("Invalid logon") {
        TestClient client("127.0.0.1", 8080);
        REQUIRE(client.Connect());

        std::string invalid_logon_msg = createFixMessage("A", {
            {hffix::tag::SenderCompID, "INVALID"},
            {hffix::tag::TargetCompID, "SERVER"},
            {hffix::tag::EncryptMethod, "1"}
        });
        REQUIRE(client.SendMessage(invalid_logon_msg));

        std::string response = client.ReceiveMessage();
        REQUIRE(response.empty());  // Expecting no response for invalid logon

        client.Close();
    }

    SECTION("New order for non-existent instrument") {
        TestClient client("127.0.0.1", 8080);
        REQUIRE(client.Connect());

        // First, send a valid logon
        std::string logon_msg = createFixMessage("A", {
            {hffix::tag::SenderCompID, "CLIENT"},
            {hffix::tag::TargetCompID, "SERVER"},
            {hffix::tag::EncryptMethod, "0"}
        });
        REQUIRE(client.SendMessage(logon_msg));
        client.ReceiveMessage();  // Consume logon response

        // Now send an order for a non-existent instrument
        std::string new_order_msg = createFixMessage("D", {
            {hffix::tag::Symbol, "INVALID"},
            {hffix::tag::Side, "1"},
            {hffix::tag::OrdType, "1"},
            {hffix::tag::Price, "15000"},
            {hffix::tag::OrderQty, "100"}
        });
        REQUIRE(client.SendMessage(new_order_msg));

        std::string response = client.ReceiveMessage();
        REQUIRE(!response.empty());
        std::map<int, std::string> reject_fields;
        REQUIRE(parseFixMessage(response, "3", reject_fields));  // Reject message

        client.Close();
    }

    exchange.Stop();
    exchange_thread.wait();
}

///
/// Client tests
///

// Mock server to simulate exchange responses
class MockServer {
public:
    MockServer(int port) : port_(port), should_run_(false) {}

    void Start() {
        should_run_ = true;
        server_thread_ = std::thread(&MockServer::Run, this);
    }

    void Stop() {
        should_run_ = false;
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }

    void SetResponse(const std::string& response) {
        response_ = response;
    }

private:
    void Run() {
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port_);

        bind(server_fd, (struct sockaddr *)&address, sizeof(address));
        listen(server_fd, 3);

        while (should_run_) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(server_fd, &readfds);
            timeval timeout{0, 100000}; // 100ms timeout

            if (select(server_fd + 1, &readfds, nullptr, nullptr, &timeout) > 0) {
                int new_socket = accept(server_fd, nullptr, nullptr);
                if (new_socket >= 0) {
                    char buffer[1024] = {0};
                    read(new_socket, buffer, 1024);
                    send(new_socket, response_.c_str(), response_.length(), 0);
                    close(new_socket);
                }
            }
        }

        close(server_fd);
    }

    int port_;
    std::atomic<bool> should_run_;
    std::thread server_thread_;
    std::string response_;
};

TEST_CASE("Client basic operations", "[Client]") {
    const int PORT = 8080;
    MockServer server(PORT);
    server.Start();

    SECTION("Start and stop client") {
        Client client;
        REQUIRE_NOTHROW(client.Start("127.0.0.1", PORT));
        REQUIRE_NOTHROW(client.Stop());
    }

    SECTION("Logon") {
        Client client;
        server.SetResponse(createFixMessage("A", {
            {hffix::tag::SenderCompID, "SERVER"},
            {hffix::tag::TargetCompID, "CLIENT"},
            {hffix::tag::EncryptMethod, "0"}
        }));
        REQUIRE_NOTHROW(client.Start("127.0.0.1", PORT));
        REQUIRE_NOTHROW(client.Logon());
        client.Stop();
    }

    server.Stop();
}

TEST_CASE("Client order operations", "[Client]") {
    const int PORT = 8080;
    MockServer server(PORT);
    server.Start();
    Client client;
    REQUIRE_NOTHROW(client.Start("127.0.0.1", PORT));

    SECTION("Place order") {
        server.SetResponse(createFixMessage("8", {
            {hffix::tag::OrderID, "12345"},
            {hffix::tag::ExecType, "0"},
            {hffix::tag::OrdStatus, "0"},
            {hffix::tag::Symbol, "AAPL"},
            {hffix::tag::Side, "1"},
            {hffix::tag::OrderQty, "100"},
            {hffix::tag::Price, "15000"}
        }));

        REQUIRE(client.PlaceOrder("AAPL", OrderSide::BID, OrderType::GOOD_TIL_CANCELED, 15000, 100));
    }

    SECTION("Cancel order") {
        server.SetResponse(createFixMessage("8", {
            {hffix::tag::OrderID, "12345"},
            {hffix::tag::ExecType, "4"},
            {hffix::tag::OrdStatus, "4"}
        }));

        REQUIRE(client.CancelOrder(12345));
    }

    SECTION("Get order status") {
        server.SetResponse(createFixMessage("8", {
            {hffix::tag::OrderID, "12345"},
            {hffix::tag::ExecType, "I"},
            {hffix::tag::OrdStatus, "0"},
            {hffix::tag::Symbol, "AAPL"},
            {hffix::tag::Side, "1"},
            {hffix::tag::OrderQty, "100"},
            {hffix::tag::CumQty, "0"},
            {hffix::tag::LeavesQty, "100"},
            {hffix::tag::Price, "15000"}
        }));

        auto order_status = client.GetOrderStatus(12345);
        REQUIRE(order_status.has_value());
        REQUIRE(order_status->GetID() == 12345);
        REQUIRE(order_status->GetStatus() == OrderStatus::OPEN);
    }

    client.Stop();
    server.Stop();
}

TEST_CASE("Client edge cases", "[Client]") {
    const int PORT = 8080;
    MockServer server(PORT);
    server.Start();
    Client client;

    SECTION("Connection failure") {
        REQUIRE_THROWS_AS(client.Start("invalid_host", PORT), std::runtime_error);
    }

    SECTION("Logon failure") {
        server.SetResponse("Invalid response");
        REQUIRE_NOTHROW(client.Start("127.0.0.1", PORT));
        REQUIRE_THROWS_AS(client.Logon(), std::runtime_error);
    }

    SECTION("Place order with invalid response") {
        REQUIRE_NOTHROW(client.Start("127.0.0.1", PORT));
        server.SetResponse("Invalid response");
        REQUIRE_FALSE(client.PlaceOrder("AAPL", OrderSide::BID, OrderType::GOOD_TIL_CANCELED, 15000, 100));
    }

    SECTION("Cancel non-existent order") {
        REQUIRE_NOTHROW(client.Start("127.0.0.1", PORT));
        server.SetResponse(createFixMessage("8", {
            {hffix::tag::OrderID, "12345"},
            {hffix::tag::ExecType, "8"},
            {hffix::tag::OrdStatus, "8"}
        }));
        REQUIRE_FALSE(client.CancelOrder(12345));
    }

    SECTION("Get status of non-existent order") {
        REQUIRE_NOTHROW(client.Start("127.0.0.1", PORT));
        server.SetResponse(createFixMessage("3", {
            {hffix::tag::OrderID, "12345"},
            {hffix::tag::Text, "Order not found"}
        }));
        REQUIRE_FALSE(client.GetOrderStatus(12345).has_value());
    }

    client.Stop();
    server.Stop();
}

TEST_CASE("Client stress test", "[Client]") {
    const int PORT = 8080;
    MockServer server(PORT);
    server.Start();
    Client client;
    REQUIRE_NOTHROW(client.Start("127.0.0.1", PORT));

    SECTION("Multiple rapid order placements") {
        server.SetResponse(createFixMessage("8", {
            {hffix::tag::OrderID, "12345"},
            {hffix::tag::ExecType, "0"},
            {hffix::tag::OrdStatus, "0"},
            {hffix::tag::Symbol, "AAPL"},
            {hffix::tag::Side, "1"},
            {hffix::tag::OrderQty, "100"},
            {hffix::tag::Price, "15000"}
        }));

        const int NUM_ORDERS = 100;
        int successful_orders = 0;
        for (int i = 0; i < NUM_ORDERS; ++i) {
            if (client.PlaceOrder("AAPL", OrderSide::BID, OrderType::GOOD_TIL_CANCELED, 15000 + i, 100)) {
                ++successful_orders;
            }
        }
        REQUIRE(successful_orders == NUM_ORDERS);
    }

    client.Stop();
    server.Stop();
}