#include "../../common/include/Tools.h"

#include "../../common/include/Serializer.h"
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <thread>
#include <algorithm>

#include <mutex>

namespace UAVhNode {

    // ============================================================
    // Global variables defined in UAVh.cpp
    // ============================================================

    extern gmp_randstate_t state;
    extern Params pp;
    extern UAV_h uavh;
    extern mpz_class message;
    extern int threshold;
    extern int numUAV;

    extern std::vector<parSig> partialSigs;

    extern std::mutex mtx;
    extern string bitmap;

    // ============================================================
    // TA communication
    // ============================================================

    using Client = websocketpp::client<websocketpp::config::asio_client>;
    using MsgClient = websocketpp::config::asio_client::message_type::ptr;
    using Server = websocketpp::server<websocketpp::config::asio>;
    using MsgServer = Server::message_ptr;
    using websocketpp::connection_hdl;

    /**
     * @brief Called when connection to TA is opened.
     *        UAVh sends its ID to request:
     *         - Public parameters,
     *         - UAVh’s transformation key,
     *         - Registered UAV identities.
     */
    void handleTAOpen(Client *c, connection_hdl hdl);

    /**
     * @brief Called when TA sends initialization data to UAVh.
     *        Receives and stores:
     *         - System parameters,
     *         - Transformation key alpha,
     *         - Registered UAV identities, threshold, message, etc.
     */
    void handleTAMessage(Client *c, connection_hdl hdl, MsgClient msg);

    /**
     * @brief Establishes a WebSocket connection to the TA.
     * @return 0 on success, -1 on connection or runtime failure.
     */
    int connectToTA();


    // ============================================================
    // UAV_i partial signatures
    // ============================================================

/**
     * @brief Callback function executed when a WebSocket connection is established with a UAV.
     *
     * This function constructs and transmits the selected signer set S to the connected UAV.
     * To optimize bandwidth utilization in constrained networks, the set S is encoded
     * as a compact bitmap (bit-array) rather than a list of full identities.
     *
     * Mechanism:
     * 1. Calculates the bitmap size based on total UAVs (N).
     * 2. Sets the bits corresponding to the selected 'threshold' (t) UAVs to 1.
     * 3. Sends the bitmap as a binary payload.
     *
     * @param c   Pointer to the WebSocket++ client endpoint instance.
     * @param hdl The handle identifying the active connection to the specific UAV.
     */
    void handleUAVOpen(Client *c, connection_hdl hdl);
    void handleUAVOpen(Client *c, connection_hdl hdl);

    /**
     * @brief Called when UAVh receives a partial signature from a UAV in S.
     *        UAVh transforms the partial signature and stores it for aggregation.
     */
    void handleUAVMessage(Client *c, connection_hdl hdl, MsgClient msg);

    /**
     * @brief Contacts all candidate UAVs in S, collects their partial signatures,
     *        transforms them, and stores them for later aggregation.
     * @return 0 on success, -1 on any communication or processing failure.
     */
    int collectPartialSignatures();


    // ============================================================
    // Verifier server
    // ============================================================

    /**
     * @brief Handles requests from the verifier.
     *        Upon receiving a request, UAVh returns the final transformed
     *        aggregated signature Sigma.
     */
    void handleVerifierMessage(Server *s, connection_hdl hdl, MsgServer msg);

    /**
     * @brief Starts a WebSocket server for verifier connections.
     *        The verifier connects to retrieve the aggregated Sigma.
     */
    void startUAVhServer();


    // ============================================================
    // Entry point
    // ============================================================

    /**
     * @brief Entry point for UAVh execution.
     *        Performs:
     *         1. TA initialization,
     *         2. Collection of partial signatures,
     *         3. Starting the verifier server.
     * @return 0 on success, -1 otherwise.
     */
    int run();

} // namespace UAVhNode
