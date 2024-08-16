#ifndef EXCHANGE_HPP
#define EXCHANGE_HPP

#include <unordered_map>
#include <shared_mutex>
#include <atomic>
#include <memory>
#include <string>

#include "utils.hpp"
#include "order.hpp"
#include "order_book.hpp"
#include "hffix.hpp"

/**
 * Constant for the maximum buffer size used in network operations.
 */
constexpr size_t BUFFER_SIZE = 1024;

/**
 * @class Exchange
 * Represents a financial exchange handling multiple instruments and order books.
 * 
 * This class manages the core functionality of the exchange, including client connections,
 * order processing, and maintaining order books for different instruments.
 */
class Exchange {
public:
    /**
     * Construct a new Exchange object.
     */
    Exchange();

    /**
     * Destroy the Exchange object and stop all operations.
     */
    ~Exchange();

    /**
     * Start the exchange on the specified port.
     * 
     * @param port The port number to listen on for incoming connections.
     * @throws std::runtime_error if the exchange fails to start.
     */
    void Start(int port);

    /**
     * Stop the exchange and all its operations.
     */
    void Stop();
    
    /**
     * Add a new instrument to the exchange.
     * 
     * @param ticker The ticker symbol of the instrument to add.
     * @throws std::runtime_error if the exchange is running.
     * @throws std::invalid_argument if the instrument already exists.
     */
    void AddInstrument(std::string ticker);
    
    /**
     * Remove an instrument from the exchange.
     * 
     * @param ticker The ticker symbol of the instrument to remove.
     * @throws std::runtime_error if the exchange is running.
     * @throws std::invalid_argument if the instrument doesn't exist.
     */
    void RemoveInstrument(std::string ticker);
private:
    /**
     * Handle a client connection.
     * 
     * @param conn The client socket descriptor.
     * @return int Returns 0 on success, -1 on failure.
     */
    int HandleClient(int conn);

    /**
     * Process a logon message from a client.
     * 
     * @param reader The FIX message reader.
     * @return true if logon is successful, false otherwise.
     */
    bool ProcessLogon(hffix::message_reader& reader);

    /**
     * Send a logon response to a client.
     * 
     * @param client_sock The client socket descriptor.
     */
    void SendLogonResponse(int client_sock);

    /**
     * Process an incoming FIX message.
     * 
     * @param reader The FIX message reader.
     * @param client_sock The client socket descriptor.
     */
    void ProcessMessage(hffix::message_reader& reader, int client_sock);

    /**
     * Process a new order request.
     * 
     * @param reader The FIX message reader.
     * @param client_sock The client socket descriptor.
     */
    void ProcessNewOrder(hffix::message_reader& reader, int client_sock);

    /**
     * Send a new order acknowledgement to a client.
     * 
     * @param client_sock The client socket descriptor.
     * @param order The new order.
     */
    void SendNewOrderAck(int client_sock, std::shared_ptr<Order>& order);

    /**
     * Process an order cancellation request.
     * 
     * @param reader The FIX message reader.
     * @param client_sock The client socket descriptor.
     */
    void ProcessCancelOrder(hffix::message_reader& reader, int client_sock);

    /**
     * Send an order cancellation acknowledgement to a client.
     * 
     * @param client_sock The client socket descriptor.
     * @param order_id The ID of the cancelled order.
     */
    void SendCancelOrderAck(int client_sock, OrderID order_id);

    /**
     * Process an order status request.
     * 
     * @param reader The FIX message reader.
     * @param client_sock The client socket descriptor.
     */
    void ProcessGetOrderStatus(hffix::message_reader& reader, int client_sock);

    /**
     * Send an order status to a client.
     * 
     * @param client_sock The client socket descriptor.
     * @param order The order whose status is being sent.
     */
    void SendOrderStatus(int client_sock, std::shared_ptr<Order>& order);

    /**
     * Send a rejection message to a client.
     * 
     * @param client_sock The client socket descriptor.
     * @param reason The reason for the rejection.
     */
    void SendRejection(int client_sock, std::string reason);

    std::atomic<bool> running_; ///< Flag indicating if the exchange is running.
    std::atomic<OrderID> next_order_id_; ///< The next available order ID.
    mutable std::shared_mutex mutex_; ///< Mutex for thread safe operations.
    std::unordered_map<OrderID, std::shared_ptr<Order>> orders_; ///< Map of all orders.
    std::unordered_map<std::string, std::unique_ptr<OrderBook>> order_books_; ///< Map of order books for each instrument.
};

#endif