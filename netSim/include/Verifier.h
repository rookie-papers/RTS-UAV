#include "../../common/include/Tools.h"
#include "../../common/include/Serializer.h"
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <thread>

#include <chrono>

/**
 * @file Verifier.h
 * @brief Header for the Verifier module.
 *
 * The verifier performs:
 *  - Registration to TA and obtaining system parameters
 *  - Contacting UAVh to obtain aggregated signature Σ
 *  - Running threshold verification Verify()
 */

namespace verifier {

    // ============================================================
    // Define concrete websocketpp client type
    // ============================================================
    using websocketpp::lib::placeholders::_1;
    using websocketpp::lib::placeholders::_2;
    using websocketpp::lib::bind;

    using Client = websocketpp::client<websocketpp::config::asio_client>;
    using MsgClient = websocketpp::config::asio_client::message_type::ptr;
    using websocketpp::connection_hdl;

    // ============================================================
    // Global state (extern declarations only)
    // ============================================================
    extern gmp_randstate_t state;
    extern Params params;
    extern int thresholdT;
    extern mpz_class messageM;
    extern mpz_class sk_v;
    extern std::vector<ECP2> PK_s;
    extern std::chrono::high_resolution_clock::time_point auth_start_time;

    // ============================================================
    // TA connection handlers
    // ============================================================

    /**
     * @brief Called when the connection to the TA is opened.
     *        Verifier sends its ID to register and request:
     *         - System public parameters,
     *         - Number of UAVs / threshold,
     *         - UAV public keys needed for verification.
     */
    void onTAOpen(Client *c, connection_hdl hdl);

    /**
     * @brief Called when TA sends initialization data.
     *        Receives:
     *         - Public parameters,
     *         - UAV public keys at threshold,
     *         - Test message,
     *         - Threshold value.
     */
    void onTAMessage(Client *c, connection_hdl hdl, MsgClient msg);

    /**
     * @brief Establishes a WebSocket connection to the TA and
     *        runs the event loop until parameters are received.
     * @return 0 on success, -1 on failure.
     */
    int connectToTA();


    // ============================================================
    // UAVh (aggregator) connection handlers
    // ============================================================

    /**
     * @brief Called when the connection to UAVh (cluster head) is opened.
     *        Verifier sends its public key PK_v.
     */
    void onUAVhOpen(Client *c, connection_hdl hdl);

    /**
     * @brief Called when UAVh sends the aggregated transformed signature.
     *        Verifier deserializes Sigma and performs threshold verification.
     */
    void onUAVhMessage(Client *c, connection_hdl hdl, MsgClient msg);

    /**
     * @brief Connects to the UAVh WebSocket server to obtain
     *        the aggregated transformed signature.
     * @return 0 on success, -1 on failure.
     */
    int connectToUAVh();

} // namespace verifier
