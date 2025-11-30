#ifndef TA_H
#define TA_H

#include "../../common/include/Tools.h"
#include "../../common/include/Serializer.h"

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>


/**
 * @file TA.h
 * @brief Declaration of Trusted Authority (TA) module.
 *
 * The TA is responsible for:
 *  - System parameter generation.
 *  - Threshold polynomial generation.
 *  - Distribution of UAV keys.
 *  - Distribution of UAVh's transformation key.
 *  - Communicating with all components via WebSocket.
 */

namespace TA {

    using websocketpp::connection_hdl;
    using Server = websocketpp::server<websocketpp::config::asio>;
    using MessagePtr = Server::message_ptr;

// ============================================================
// Global state (isolated inside namespace TA)
// ============================================================

    extern gmp_randstate_t state;
    extern Params pp;
    extern mpz_class alpha;

    extern std::vector<mpz_class> poly_d;
    extern std::vector<mpz_class> poly_b;

    extern mpz_class messageM;
    extern int thresholdT;

    extern std::vector<mpz_class> uavIDs;
    extern std::vector<ECP2> uavPKs_t;


    /**
     * @brief Initialize TA internal parameters.
     *        (System setup, polynomial generation, threshold computation)
     */
    void initParams();

    /**
     * @brief WebSocket message handler for UAV/UAVh/Verifier registration.
     *
     * @param server  Pointer to WebSocket server instance.
     * @param hdl     Connection handle.
     * @param msg     Received message pointer.
     */
    void onRegister(Server* server, connection_hdl hdl, MessagePtr msg);

    /**
     * @brief Start the TA WebSocket server (listens on port 9002).
     *
     * @return 0 on success, -1 on failure.
     */
    int startServer();

    /**
     * @brief Complete running entry for TA.
     *        Calls initParams() then startServer().
     *
     * @return 0 on success, -1 on failure.
     */
    int run();

} // namespace TA

#endif // TA_H
