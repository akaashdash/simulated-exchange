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

/**
 * Constant for the maximum buffer size used in network operations.
 */
constexpr size_t BUFFER_SIZE = 1024;

/**
 * @class Client
 * Represents a client that can connect to and interact with an exchange.
 *
 * This class provides functionality to connect to an exchange, place orders,
 * cancel orders, and retrieve order status information.
 */
class Client {
public:
    /**
     * Construct a new Client object.
     */
    Client();

    /**
     * Destroy the Client object and stop all operations.
     */
    ~Client();

    /**
     * Connects the client to the specified exchange.
     * 
     * @param exchange_host The hostname or IP address of the exchange.
     * @param exchange_port The port number on which the exchange is listening.
     * @throws std::runtime_error If connection fails.
     * @throws std::invalid_argument If invalid host given.
     */
    void Start(std::string exchange_host, int exchange_port);

    /**
     * Disconnects the client from the exchange.
     */
    void Stop();

    /**
     * Performs the logon process with the exchange.
     * 
     * @throws std::runtime_error If logon fails.
     */
    void Logon();

    /**
     * Places a new order on the exchange.
     * 
     * @param ticker The ticker symbol of the instrument.
     * @param side The side of the order.
     * @param type The type of the order.
     * @param price The price of the order.
     * @param quantity The quantity of the order.
     * @return true if the order was successfully placed, false otherwise.
     */
    bool PlaceOrder(std::string ticker, OrderSide side, OrderType type, OrderPrice price, OrderQuantity quantity);

    /**
     * Cancels an existing order on the exchange.
     * 
     * @param id The ID of the order to cancel.
     * @return true if the order was successfully cancelled, false otherwise.
     */
    bool CancelOrder(OrderID id);

    /**
     * Retrieves the current status of an order.
     * 
     * @param id The ID of the order to query.
     * @return An optional containing the Order if found, or empty if not found.
     */
    std::optional<Order> GetOrderStatus(OrderID id);
private:
    int client_sock_; ///< The socket descriptor for the client connection.
    std::unordered_set<OrderID> orders_; ///< Set of order IDs placed by this client.
};

#endif