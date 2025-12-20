#include "../../common/include/Tools.h"

#include "../../common/include/Serializer.h"

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <thread>
#include <algorithm>


/**
 * @file UAV.h
 * @brief Declarations for the UAV node.
 *
 * This header mirrors the implementation in src/UAV.cpp.
 * The websocketpp type aliases are declared here so that
 * function signatures match exactly between declaration and definition.
 */

namespace UAVNode_NS {

    // ------------------------------
    // websocketpp concrete aliases
    // ------------------------------
    using Client     = websocketpp::client<websocketpp::config::asio_client>;
    using MsgClient  = websocketpp::config::asio_client::message_type::ptr;
    using Server     = websocketpp::server<websocketpp::config::asio>;
    using MsgServer  = Server::message_ptr;
    using websocketpp::connection_hdl;

    // ------------------------------
    // Global state (defined in UAV.cpp)
    // ------------------------------
    extern gmp_randstate_t state;
    extern Params          pp;        // public parameters from TA
    extern UAV             uav;       // UAV's private information (struct defined in common)
    extern mpz_class       message;   // message M
    extern int             threshold; // threshold t
    extern vector<mpz_class> registeredIDs;

    // ------------------------------
    // TA connection handlers (client mode)
    // ------------------------------

    /**
     * @brief Called when UAV successfully connects to TA.
     *
     * @param c   pointer to websocketpp client instance
     * @param hdl connection handle
     */
    void handleTAOpen(Client* c, connection_hdl hdl);

    /**
     * @brief Called when TA sends UAV's key and system parameters.
     *
     * @param c   pointer to websocketpp client instance
     * @param hdl connection handle
     * @param msg received message pointer
     */
    void handleTAMessage(Client* c, connection_hdl hdl, MsgClient msg);

    /**
     * @brief Connect to TA (ws://ip:9002) and receive parameters.
     *
     * @return 0 on success, -1 on failure
     */
    int connectToTA();


    // ------------------------------
    // UAV server handlers (server mode)
    // ------------------------------

/**
     * @brief Callback function executed when a message (signer set) is received from UAVh.
     *
     * This function handles the "Sign Request" logic on the UAV side.
     * It receives the selected signer set encoded as a compact bitmap to save bandwidth.
     *
     * Workflow:
     * 1. **Self-Check**: Checks if the local UAV is selected using bitwise operations on the received payload (O(1) complexity).
     * 2. **Set Reconstruction**: If selected, iterates through the bitmap to reconstruct the full
     * `signerSet` (vector of IDs) using the local `registeredIDs` lookup table.
     * This is required to calculate Lagrange coefficients during signing.
     * 3. **Signing**: Generates a partial signature using the reconstructed set and local private key.
     * 4. **Response**: Sends the partial signature string (or "null" if not selected) back to UAVh.
     *
     * @param server Pointer to the WebSocket++ server endpoint instance.
     * @param hdl    The handle identifying the connection to UAVh.
     * @param msg    The received message object containing the binary bitmap payload.
     */
    void serverOnMessage(Server* server, connection_hdl hdl, MsgServer msg);

    /**
     * @brief Start UAV server to listen for UAVh (port provided by main).
     *
     * @param port listening port
     */
    void startUAVServer();


    // ------------------------------
    // Entry point for UAV process
    // ------------------------------

    /**
     * @brief High-level run: register with TA, then start server.
     *
     * @param port server listening port for UAVh
     * @return 0 on success, -1 on failure
     */
    int run(int port);

} // namespace UAVNode

