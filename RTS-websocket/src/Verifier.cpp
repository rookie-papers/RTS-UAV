#include "../include/Verifier.h"

namespace verifier {

// ============================================================
// Global state for verifier (inside namespace)
// ============================================================
    gmp_randstate_t state;
    Params params;
    int thresholdT = 0;
    mpz_class messageM;
    mpz_class sk_v;             // verifier's secret
    std::vector<ECP2> PK_s;     // public keys of UAVs at threshold T


// ============================================================
// TA connection callbacks
// ============================================================
/**
 * @brief Called when connection to TA is opened.
 *        Send verifier ID to TA to register and request params.
 */
    void onTAOpen(Client *c, connection_hdl hdl) {
        initState(state);

        // Use a fixed ID for verifier/test (same as other components)
        mpz_class verID =
                0x6666666666666666666666666666666666666666666666666666666666666666_mpz;
        std::string idStr = mpz_to_str(verID);

        websocketpp::lib::error_code ec;
        c->send(hdl, idStr, websocketpp::frame::opcode::text, ec);

        if (ec) {
            std::cerr << "[Verifier] Failed to send ID to TA: " << ec.message() << std::endl;
        } else {
            std::cout << "[Verifier] Sent ID to TA." << std::endl;
        }
    }


/**
 * @brief Called when TA sends registration package.
 *        Deserialize and store params needed for verification.
 */
    void onTAMessage(Client *c, connection_hdl hdl, MsgClient msg) {
        std::string payload = msg->get_payload();
        std::cout << "[Verifier] Received TA package." << std::endl;

        TransmissionPackage pkg = str_to_Package(payload);

        params = pkg.pp;
        PK_s = pkg.uav.PK;   // store UAV PK fragments
        messageM = pkg.M;
        thresholdT = pkg.t;

        // Close connection to TA after receiving parameters
        c->close(hdl, websocketpp::close::status::normal, "TA done");
    }


// ============================================================
// Connect to TA helper
// ============================================================
    int connectToTA() {
        Client client;
        const std::string uri = "ws://localhost:9002";

        try {
            client.set_access_channels(websocketpp::log::alevel::none);
            client.init_asio();

            client.set_open_handler(bind(&onTAOpen, &client, _1));
            client.set_message_handler(bind(&onTAMessage, &client, _1, _2));

            websocketpp::lib::error_code ec;
            auto con = client.get_connection(uri, ec);
            if (ec) {
                std::cerr << "[Verifier] Connect to TA failed: " << ec.message() << std::endl;
                return -1;
            }

            client.connect(con);

            std::thread th([&client]() { client.run(); });
            if (th.joinable()) th.join();
        } catch (const std::exception &e) {
            std::cerr << "[Verifier] Exception: " << e.what() << std::endl;
            return -1;
        }

        return 0;
    }


// ============================================================
// UAVh connection callbacks (contact aggregator to obtain aggregated sigma)
// ============================================================
/**
 * @brief Called when connection to UAVh is opened.
 *        Send verifier public key PK_v so UAVh can produce aggregated Sigma.
 */
    void onUAVhOpen(Client *c, connection_hdl hdl) {
        initState(state);

        sk_v = rand_mpz(state);
        mpz_class PK_v = pow_mpz(params.g, sk_v, params.q); // g^sk mod q
        std::string pkStr = mpz_to_str(PK_v);

        websocketpp::lib::error_code ec;
        c->send(hdl, pkStr, websocketpp::frame::opcode::text, ec);

        if (ec) {
            std::cerr << "[Verifier] Failed to send PK_v to UAVh: " << ec.message() << std::endl;
        } else {
            std::cout << "[Verifier] Sent PK_v to UAVh." << std::endl;
        }
    }


/**
 * @brief Called when aggregated signature is returned from UAVh.
 *        Deserialize Sigma and run verification.
 */
    void onUAVhMessage(Client *c, connection_hdl hdl, MsgClient msg) {
        std::string payload = msg->get_payload();
        std::cout << "[Verifier] Received aggregated signature from UAVh." << std::endl;

        Sigma sigma = str_to_Sigma(payload);

        printLine("Sigma received");
        showSigma(sigma);

        printLine("Verifier secret sk_v");
        show_mpz(sk_v.get_mpz_t());

        int res = Verify(sigma, sk_v, params, messageM, thresholdT, PK_s);
        std::cout << "[Verifier] verify result = " << res << std::endl;

        // Close connection so client run() returns
        c->close(hdl, websocketpp::close::status::normal, "done");
    }


// ============================================================
// Connect to UAVh helper
// ============================================================
    int connectToUAVh() {
        Client client;
        const std::string uri = "ws://localhost:8001";

        try {
            client.set_access_channels(websocketpp::log::alevel::none);
            client.init_asio();

            client.set_open_handler(bind(&onUAVhOpen, &client, _1));
            client.set_message_handler(bind(&onUAVhMessage, &client, _1, _2));

            websocketpp::lib::error_code ec;
            auto con = client.get_connection(uri, ec);
            if (ec) {
                std::cerr << "[Verifier] Connect to UAVh failed: " << ec.message() << std::endl;
                return -1;
            }

            client.connect(con);

            std::thread th([&client]() { client.run(); });
            if (th.joinable()) th.join();
        } catch (const std::exception &e) {
            std::cerr << "[Verifier] Exception: " << e.what() << std::endl;
            return -1;
        }

        return 0;
    }

} // namespace verifier


// ============================================================
// Program entry
// ============================================================
int main() {
    // 1. Get params from TA
    if (verifier::connectToTA() != 0) {
        std::cerr << "[Main] Failed to connect to TA" << std::endl;
        return -1;
    }

    // 2. Contact UAVh to obtain Sigma and verify
    if (verifier::connectToUAVh() != 0) {
        std::cerr << "[Main] Failed to connect to UAVh" << std::endl;
        return -1;
    }

    return 0;
}
