#include "../include/Verifier.h"

namespace verifier_NS {

// ============================================================
// Global state for verifier (inside namespace)
// ============================================================
    gmp_randstate_t state;
    Params params;
    int thresholdT = 0;
    mpz_class messageM;
    mpz_class sk_v;             // verifier's secret
    std::vector<ECP2> PK_s;     // public keys of UAVs at threshold T
    std::chrono::high_resolution_clock::time_point auth_start_time;
    vector<mpz_class> registeredIDs;

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

        registeredIDs = pkg.registeredIDs;
        // Close connection to TA after receiving parameters
        c->close(hdl, websocketpp::close::status::normal, "TA done");
    }


// ============================================================
// Connect to TA helper
// ============================================================
    int connectToTA() {
        Client client;
        const std::string uri = "ws://10.0.10.2:9002";

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


    // Helper function: Converts a binary string to a Hex string to ensure safe transmission
    std::string stringToHex(const std::string& input) {
        static const char* const lut = "0123456789ABCDEF";
        size_t len = input.length();
        std::string output;
        output.reserve(2 * len);
        for (size_t i = 0; i < len; ++i) {
            const unsigned char c = input[i];
            output.push_back(lut[c >> 4]);
            output.push_back(lut[c & 15]);
        }
        return output;
    }

// ============================================================
// UAVh connection callbacks
// ============================================================

    void onUAVhOpen(Client *c, connection_hdl hdl) {
        auth_start_time = std::chrono::high_resolution_clock::now();
        initState(state);

        // 1. Generate Verifier's ephemeral public/private key pair
        sk_v = rand_mpz(state);
        mpz_class PK_v = pow_mpz(params.g, sk_v, params.q);
        std::string pkStr = mpz_to_str(PK_v);

        // 2. Generate a random bitmap (selecting 't' UAVs out of 'n')
        int n = params.n;
        int t = params.tm;

        // Create a list of indices [0, n-1] and shuffle them to select random signers
        std::vector<int> indices(n);
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), std::mt19937(std::random_device()()));

        // Initialize the bitmap with zeros
        int bitmapSize = (n + 7) / 8;
        std::string bitmap(bitmapSize, 0);

        // Set the bits for the first 't' indices from the shuffled list
        for(int i = 0; i < t; ++i) {
            int idx = indices[i];
            int byteIndex = idx / 8;
            int bitIndex  = idx % 8;
            bitmap[byteIndex] |= (1 << bitIndex);
        }

        // 3. Construct the payload: PK_v # Hex(Bitmap)
        std::string bitmapHex = stringToHex(bitmap);
        std::string msg = pkStr + "#" + bitmapHex;

        websocketpp::lib::error_code ec;
        c->send(hdl, msg, websocketpp::frame::opcode::text, ec);

        if (ec) {
            std::cerr << "[Verifier] Failed to send Challenge (PK+Bitmap): " << ec.message() << std::endl;
        } else {
            std::cout << "[Verifier] Sent Challenge to UAVh (t=" << t << ")." << std::endl;
        }
    }

    void onUAVhMessage(Client *c, connection_hdl hdl, MsgClient msg) {
        std::string payload = msg->get_payload();
        std::cout << "[Verifier] Received aggregated signature from UAVh." << std::endl;

        Sigma sigma = str_to_Sigma(payload);
        int res = Verify(sigma, sk_v, params, messageM, registeredIDs, PK_s);

        auto auth_end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(auth_end_time - auth_start_time).count();
        std::cout << "[Verifier] verify result = " << res << std::endl;
        std::cout << ">>> Total Authentication Time: " << duration << " ms <<<" << std::endl;

        c->close(hdl, websocketpp::close::status::normal, "done");
    }



// ============================================================
// Connect to UAVh helper
// ============================================================
    int connectToUAVh() {
        Client client;
        const std::string uri = "ws://10.0.30.2:8001";

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
    if (verifier_NS::connectToTA() != 0) {
        std::cerr << "[Main] Failed to connect to TA" << std::endl;
        return -1;
    }

    // 2. Contact UAVh to obtain Sigma and verify
    if (verifier_NS::connectToUAVh() != 0) {
        std::cerr << "[Main] Failed to connect to UAVh" << std::endl;
        return -1;
    }

    return 0;
}
