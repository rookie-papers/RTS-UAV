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
     * @brief Handle a request from UAVh: receive signer-set S and reply with partial signature.
     *
     * @param server pointer to websocketpp server instance
     * @param hdl    connection handle
     * @param msg    received message pointer
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

