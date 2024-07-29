#include "catch.hpp"

#include "order_book.hpp"
#include "order.hpp"

#include <memory>
#include <chrono>
#include <thread>
#include <iostream>
#include <limits>
#include <cmath>

TEST_CASE("Order functionality", "[order]") {
    SECTION("Constructor and getters") {
        auto order = std::make_shared<Order>(1, 100, 10, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        
        REQUIRE(order->GetID() == 1);
        REQUIRE(order->GetPrice() == 100);
        REQUIRE(order->GetQuantity() == 10);
        REQUIRE(order->GetRemaining() == 10);
        REQUIRE(order->GetFilled() == 0);
        REQUIRE(order->GetSide() == OrderSide::BID);
        REQUIRE(order->GetType() == OrderType::GOOD_TIL_CANCELED);
        REQUIRE_FALSE(order->IsFilled());
    }

    SECTION("Fill functionality") {
        auto order = std::make_shared<Order>(1, 100, 10, OrderSide::ASK, OrderType::FILL_OR_KILL);
        
        REQUIRE(order->GetRemaining() == 10);
        order->Fill(5);
        REQUIRE(order->GetRemaining() == 5);
        REQUIRE(order->GetFilled() == 5);
        order->Fill(5);
        REQUIRE(order->GetRemaining() == 0);
        REQUIRE(order->GetFilled() == 10);
        REQUIRE(order->IsFilled());
        REQUIRE_THROWS_AS(order->Fill(1), std::invalid_argument);
    }

    SECTION("Creation timestamp") {
        auto start = CurrentTime();
        auto order = std::make_shared<Order>(1, 100, 10, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        auto end = CurrentTime();
        REQUIRE(order->GetCreatedAt() >= start);
        REQUIRE(order->GetCreatedAt() <= end);
    }

    SECTION("Different order types") {
        auto gtc = std::make_shared<Order>(1, 100, 10, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        auto fok = std::make_shared<Order>(2, 100, 10, OrderSide::BID, OrderType::FILL_OR_KILL);
        auto ioc = std::make_shared<Order>(3, 100, 10, OrderSide::BID, OrderType::IMMEDIATE_OR_CANCEL);

        REQUIRE(gtc->GetType() == OrderType::GOOD_TIL_CANCELED);
        REQUIRE(fok->GetType() == OrderType::FILL_OR_KILL);
        REQUIRE(ioc->GetType() == OrderType::IMMEDIATE_OR_CANCEL);
    }
}

TEST_CASE("PriceLevel functionality", "[price_level]") {
    PriceLevel level;
    auto order1 = std::make_shared<Order>(1, 100, 10, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
    auto order2 = std::make_shared<Order>(2, 100, 5, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
    
    SECTION("Adding orders") {
        REQUIRE(level.IsEmpty());
        level.Add(order1);
        REQUIRE_FALSE(level.IsEmpty());
        REQUIRE(level.GetTotalQuantity() == 10);
        level.Add(order2);
        REQUIRE(level.GetTotalQuantity() == 15);
        REQUIRE_THROWS_AS(level.Add(order1), std::invalid_argument);
    }
    
    SECTION("Removing orders") {
        level.Add(order1);
        level.Add(order2);
        level.Remove(order1->GetID());
        REQUIRE(level.GetTotalQuantity() == 5);
        REQUIRE_THROWS_AS(level.Remove(order1->GetID()), std::invalid_argument);
        level.Remove(order2->GetID());
        REQUIRE(level.IsEmpty());
    }
    
    SECTION("Filling orders") {
        level.Add(order1);
        level.Add(order2);
        REQUIRE(level.CanFill(10));
        REQUIRE(level.CanFill(15));
        REQUIRE_FALSE(level.CanFill(16));
        
        auto fill_order = std::make_shared<Order>(3, 100, 12, OrderSide::ASK, OrderType::IMMEDIATE_OR_CANCEL);
        level.Fill(fill_order);
        REQUIRE(level.GetTotalQuantity() == 3);
        REQUIRE(fill_order->GetFilled() == 12);
        REQUIRE(order1->IsFilled());
        REQUIRE(order2->GetFilled() == 2);
    }
}

TEST_CASE("OrderBook functionality", "[order_book]") {
    OrderBook book;
    
    SECTION("Placing orders") {
        auto bid1 = std::make_shared<Order>(1, 100, 10, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        auto bid2 = std::make_shared<Order>(2, 101, 5, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        auto ask1 = std::make_shared<Order>(3, 102, 7, OrderSide::ASK, OrderType::GOOD_TIL_CANCELED);
        auto ask2 = std::make_shared<Order>(4, 103, 3, OrderSide::ASK, OrderType::GOOD_TIL_CANCELED);
        
        REQUIRE(book.PlaceOrder(bid1));
        REQUIRE(book.PlaceOrder(bid2));
        REQUIRE(book.PlaceOrder(ask1));
        REQUIRE(book.PlaceOrder(ask2));
        REQUIRE_THROWS_AS(book.PlaceOrder(bid1), std::invalid_argument); // Duplicate order ID
    }
    
    SECTION("Cancelling orders") {
        auto order = std::make_shared<Order>(1, 100, 10, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        book.PlaceOrder(order);
        REQUIRE(book.CancelOrder(order->GetID()));
        REQUIRE_THROWS_AS(book.CancelOrder(order->GetID()), std::invalid_argument);
        REQUIRE_THROWS_AS(book.CancelOrder(999), std::invalid_argument); // Non-existent order
    }
    
    SECTION("Matching orders") {
        auto bid1 = std::make_shared<Order>(1, 100, 10, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        auto bid2 = std::make_shared<Order>(2, 99, 5, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        auto ask1 = std::make_shared<Order>(3, 100, 7, OrderSide::ASK, OrderType::IMMEDIATE_OR_CANCEL);
        auto ask2 = std::make_shared<Order>(4, 101, 5, OrderSide::ASK, OrderType::GOOD_TIL_CANCELED);
        
        book.PlaceOrder(bid1);
        book.PlaceOrder(bid2);
        book.PlaceOrder(ask2);
        
        REQUIRE(book.PlaceOrder(ask1));
        REQUIRE(bid1->GetFilled() == 7);
        REQUIRE(ask1->IsFilled());
        REQUIRE(bid2->GetFilled() == 0);
        REQUIRE(ask2->GetFilled() == 0);
    }
    
    SECTION("Fill or Kill order") {
        auto bid1 = std::make_shared<Order>(1, 100, 10, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        auto fok_ask = std::make_shared<Order>(2, 100, 15, OrderSide::ASK, OrderType::FILL_OR_KILL);
        
        book.PlaceOrder(bid1);
        REQUIRE_FALSE(book.PlaceOrder(fok_ask));
        REQUIRE(bid1->GetFilled() == 0);
        REQUIRE(fok_ask->GetFilled() == 0);
        
        auto fok_ask2 = std::make_shared<Order>(3, 100, 10, OrderSide::ASK, OrderType::FILL_OR_KILL);
        REQUIRE(book.PlaceOrder(fok_ask2));
        REQUIRE(bid1->IsFilled());
        REQUIRE(fok_ask2->IsFilled());
    }
    
    SECTION("Fill and Kill order") {
        auto bid1 = std::make_shared<Order>(1, 100, 10, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        auto fak_ask = std::make_shared<Order>(2, 100, 15, OrderSide::ASK, OrderType::IMMEDIATE_OR_CANCEL);
        
        book.PlaceOrder(bid1);
        REQUIRE(book.PlaceOrder(fak_ask));
        REQUIRE(bid1->IsFilled());
        REQUIRE(fak_ask->GetFilled() == 10);
        REQUIRE_THROWS_AS(book.CancelOrder(fak_ask->GetID()), std::invalid_argument); // FAK order should not be in the book
    }
    
    SECTION("Can Fill check") {
        auto bid1 = std::make_shared<Order>(1, 100, 10, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        auto bid2 = std::make_shared<Order>(2, 99, 5, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        
        book.PlaceOrder(bid1);
        book.PlaceOrder(bid2);
        
        REQUIRE(book.CanFill(std::make_shared<Order>(3, 100, 10, OrderSide::ASK, OrderType::FILL_OR_KILL)));
        REQUIRE(book.CanFill(std::make_shared<Order>(4, 99, 15, OrderSide::ASK, OrderType::FILL_OR_KILL)));
        REQUIRE_FALSE(book.CanFill(std::make_shared<Order>(5, 99, 16, OrderSide::ASK, OrderType::FILL_OR_KILL)));
        REQUIRE_FALSE(book.CanFill(std::make_shared<Order>(6, 101, 1, OrderSide::ASK, OrderType::FILL_OR_KILL)));
    }
}

TEST_CASE("OrderBook edge cases 2", "[order_book]") {
    OrderBook book;
    
    SECTION("Empty book behavior") {
        REQUIRE_FALSE(book.CanFill(std::make_shared<Order>(1, 100, 1, OrderSide::ASK, OrderType::FILL_OR_KILL)));
        REQUIRE_FALSE(book.CanFill(std::make_shared<Order>(2, 100, 1, OrderSide::BID, OrderType::FILL_OR_KILL)));
        REQUIRE_THROWS_AS(book.CancelOrder(1), std::invalid_argument);
    }
    
    SECTION("Maximum price and quantity") {
        auto max_price_order = std::make_shared<Order>(1, std::numeric_limits<OrderPrice>::max(), 1, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        auto max_quantity_order = std::make_shared<Order>(2, 100, std::numeric_limits<OrderQuantity>::max(), OrderSide::ASK, OrderType::GOOD_TIL_CANCELED);
        
        REQUIRE(book.PlaceOrder(max_price_order));
        REQUIRE(book.PlaceOrder(max_quantity_order));
    }
    
    SECTION("Minimum price and quantity") {
        auto min_price_order = std::make_shared<Order>(1, 1, 1, OrderSide::ASK, OrderType::GOOD_TIL_CANCELED);
        auto min_quantity_order = std::make_shared<Order>(2, 100, 1, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        
        REQUIRE(book.PlaceOrder(min_price_order));
        REQUIRE(book.PlaceOrder(min_quantity_order));
    }
}

TEST_CASE("Order edge cases", "[order][edge]") {
    SECTION("Zero quantity order") {
        REQUIRE_THROWS_AS(std::make_shared<Order>(1, 100, 0, OrderSide::BID, OrderType::GOOD_TIL_CANCELED), std::invalid_argument);
    }

    SECTION("Maximum possible quantity") {
        auto order = std::make_shared<Order>(1, 100, std::numeric_limits<OrderQuantity>::max(), OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(order->GetQuantity() == std::numeric_limits<OrderQuantity>::max());
    }

    SECTION("Minimum and maximum prices") {
        auto min_price_order = std::make_shared<Order>(1, std::numeric_limits<OrderPrice>::min(), 1, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        auto max_price_order = std::make_shared<Order>(2, std::numeric_limits<OrderPrice>::max(), 1, OrderSide::ASK, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(min_price_order->GetPrice() == std::numeric_limits<OrderPrice>::min());
        REQUIRE(max_price_order->GetPrice() == std::numeric_limits<OrderPrice>::max());
    }

    SECTION("Filling more than remaining quantity") {
        auto order = std::make_shared<Order>(1, 100, 10, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        order->Fill(5);
        REQUIRE_THROWS_AS(order->Fill(6), std::invalid_argument);
    }
}

TEST_CASE("PriceLevel edge cases", "[price_level][edge]") {
    PriceLevel level;

    SECTION("Adding duplicate order") {
        auto order = std::make_shared<Order>(1, 100, 10, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        level.Add(order);
        REQUIRE_THROWS_AS(level.Add(order), std::invalid_argument);
    }

    SECTION("Removing non-existent order") {
        REQUIRE_THROWS_AS(level.Remove(999), std::invalid_argument);
    }

    SECTION("Filling empty level") {
        auto fill_order = std::make_shared<Order>(1, 100, 10, OrderSide::ASK, OrderType::IMMEDIATE_OR_CANCEL);
        level.Fill(fill_order);
        REQUIRE(fill_order->GetFilled() == 0);
    }

    SECTION("Maximum number of orders") {
        for (OrderID i = 0; i < 1000000; ++i) {
            auto order = std::make_shared<Order>(i, 100, 1, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
            level.Add(order);
        }
        REQUIRE(level.GetTotalQuantity() == 1000000);
    }
}

TEST_CASE("OrderBook edge cases", "[order_book][edge]") {
    OrderBook book;

    SECTION("Placing order with existing ID") {
        auto order1 = std::make_shared<Order>(1, 100, 10, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        auto order2 = std::make_shared<Order>(1, 101, 5, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(book.PlaceOrder(order1));
        REQUIRE_THROWS_AS(book.PlaceOrder(order2), std::invalid_argument);
    }

    SECTION("Cancelling non-existent order") {
        REQUIRE_THROWS_AS(book.CancelOrder(999), std::invalid_argument);
    }

    SECTION("Matching with price of zero") {
        auto zero_price_bid = std::make_shared<Order>(1, 0, 10, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        auto zero_price_ask = std::make_shared<Order>(2, 0, 10, OrderSide::ASK, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(book.PlaceOrder(zero_price_bid));
        REQUIRE(book.PlaceOrder(zero_price_ask));
        REQUIRE(zero_price_bid->IsFilled());
        REQUIRE(zero_price_ask->IsFilled());
    }
}

TEST_CASE("Unexpected scenarios", "[unexpected]") {
    OrderBook book;

    SECTION("Extreme price differences") {
        auto low_bid = std::make_shared<Order>(1, 1, 10, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        auto high_ask = std::make_shared<Order>(2, std::numeric_limits<OrderPrice>::max(), 10, OrderSide::ASK, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(book.PlaceOrder(low_bid));
        REQUIRE(book.PlaceOrder(high_ask));
        REQUIRE_FALSE(low_bid->IsFilled());
        REQUIRE_FALSE(high_ask->IsFilled());
    }

    SECTION("Fill or Kill with partial fill available") {
        auto bid1 = std::make_shared<Order>(1, 100, 5, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        auto bid2 = std::make_shared<Order>(2, 99, 4, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        auto fok_ask = std::make_shared<Order>(3, 99, 10, OrderSide::ASK, OrderType::FILL_OR_KILL);
        
        REQUIRE(book.PlaceOrder(bid1));
        REQUIRE(book.PlaceOrder(bid2));
        REQUIRE_FALSE(book.PlaceOrder(fok_ask));
        REQUIRE_FALSE(bid1->IsFilled());
        REQUIRE_FALSE(bid2->IsFilled());
        REQUIRE_FALSE(fok_ask->IsFilled());
    }

        SECTION("Cross price fills") {
        auto bid1 = std::make_shared<Order>(1, 100, 5, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        auto bid2 = std::make_shared<Order>(2, 99, 5, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        auto fok_ask = std::make_shared<Order>(3, 99, 10, OrderSide::ASK, OrderType::FILL_OR_KILL);
        
        REQUIRE(book.PlaceOrder(bid1));
        REQUIRE(book.PlaceOrder(bid2));
        REQUIRE(book.PlaceOrder(fok_ask));
        REQUIRE(bid1->IsFilled());
        REQUIRE(bid2->IsFilled());
        REQUIRE(fok_ask->IsFilled());
    }

    SECTION("Floating point price comparison") {
        auto bid = std::make_shared<Order>(1, static_cast<OrderPrice>(100.01 * 100), 10, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
        auto ask = std::make_shared<Order>(2, static_cast<OrderPrice>(100.01 * 100), 10, OrderSide::ASK, OrderType::GOOD_TIL_CANCELED);
        REQUIRE(book.PlaceOrder(bid));
        REQUIRE(book.PlaceOrder(ask));
        REQUIRE(bid->IsFilled());
        REQUIRE(ask->IsFilled());
    }
}

TEST_CASE("Stress testing", "[stress]") {
    OrderBook book;

    SECTION("Large number of orders at same price") {
        const int NUM_ORDERS = 100;
        for (int i = 0; i < NUM_ORDERS; ++i) {
            auto order = std::make_shared<Order>(i, 100, 1, OrderSide::BID, OrderType::GOOD_TIL_CANCELED);
            REQUIRE(book.PlaceOrder(order));
        }
        auto matching_order = std::make_shared<Order>(NUM_ORDERS, 100, NUM_ORDERS, OrderSide::ASK, OrderType::IMMEDIATE_OR_CANCEL);
        REQUIRE(book.PlaceOrder(matching_order));
        REQUIRE(matching_order->IsFilled());
    }

    SECTION("Alternating bids and asks") {
        const int NUM_ORDERS = 100;
        for (int i = 0; i < NUM_ORDERS; ++i) {
            auto order = std::make_shared<Order>(i, 100, 1, i % 2 == 0 ? OrderSide::BID : OrderSide::ASK, OrderType::GOOD_TIL_CANCELED);
            REQUIRE(book.PlaceOrder(order));
        }
        for (int i = 0; i < NUM_ORDERS; ++i) {
            REQUIRE_THROWS_AS(book.CancelOrder(i), std::invalid_argument);
        }
    }
}