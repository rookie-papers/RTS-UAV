#include "../include/TA.h"
#include <fstream>
#include <sstream>
#include <algorithm> // for remove_if


namespace TA {

// ============================================================
// Constants
// ============================================================

    int kNumUAV = 2;       // Total number of UAVs
    int kThresholdMax = 2; // Max threshold parameter

// ID used to distinguish cluster head UAVh
    const std::string kUAVhID = "6666666666666666666666666666666666666666666666666666666666666666";

// ============================================================
// Global state (isolated inside namespace TA)
// ============================================================

    gmp_randstate_t state;
    Params pp;                       // System public parameters
    mpz_class alpha;                 // UAV_h's transformation key

    std::vector<mpz_class> poly_d;   // Polynomial d for key distribution
    std::vector<mpz_class> poly_b;   // Polynomial b for key distribution and PK construction

    mpz_class messageM;              // Test message to be signed
    int thresholdT = 0;              // Threshold t = TM/2 + 1

    std::vector<mpz_class> uavIDs;   // All UAV IDs that have registered
    std::vector<ECP2> uavPKs_t;      // All UAV public key at threshold t


    // ============================================================
    // Helper: Trim string
    // ============================================================
    std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\n\r");
        if (std::string::npos == first) return str;
        size_t last = str.find_last_not_of(" \t\n\r");
        return str.substr(first, (last - first + 1));
    }

    // ============================================================
    // Implementation of LoadConfig
    // ============================================================
    void LoadConfig(const std::string& configPath) {
        std::ifstream file(configPath);
        if (!file.is_open()) {
            std::cerr << "[Config] Warning: Could not open config file: " << configPath
                      << ". Using default values." << std::endl;
            return;
        }

        std::string line;
        std::cout << "[Config] Loading parameters from " << configPath << "..." << std::endl;

        while (std::getline(file, line)) {
            // Remove the annotations
            size_t commentPos = line.find('#');
            if (commentPos != std::string::npos) {
                line = line.substr(0, commentPos);
            }

            if (line.find("NUM_UAV=") != std::string::npos) {
                std::string valStr = line.substr(line.find('=') + 1);
                try {
                    kNumUAV = std::stoi(trim(valStr));
                    std::cout << "  -> Set kNumUAV = " << kNumUAV << std::endl;
                } catch (...) { }
            }
            else if (line.find("THRESHOLD_M=") != std::string::npos) {
                std::string valStr = line.substr(line.find('=') + 1);
                try {
                    kThresholdMax = std::stoi(trim(valStr));
                    std::cout << "  -> Set kThresholdMax = " << kThresholdMax << std::endl;
                } catch (...) { }
            }
        }
        file.close();
    }

// ============================================================
// Initialize system parameters
// ============================================================
    void initParams() {
        std::cout << "[TA] Initializing system parameters..." << std::endl;

        initState(state);

        // Setup returns public params + generates UAV_h's conversion key masterSK
        pp = Setup(alpha, kNumUAV, kThresholdMax);

        // Generate threshold polynomial coefficients
        poly_d.reserve(kThresholdMax - 1);
        poly_b.reserve(kThresholdMax - 1);

        for (int i = 0; i < kThresholdMax - 1; ++i) {
            poly_d.push_back(rand_mpz(state));
            poly_b.push_back(rand_mpz(state));
        }

        // Construct the global public key PK
        pp.PK = getPK(poly_b);

        // Compute threshold value t
        thresholdT = kThresholdMax / 2 + 1;
        // thresholdT = kThresholdMax - 2;

        // Example message to sign
        messageM = 123456789;

        std::cout << "[TA] Initialization complete. Threshold t = "
                  << thresholdT << std::endl;
    }


// ============================================================
// Handle UAV / UAVh / Verifier registration requests
// ============================================================
    void onRegister(Server *server, connection_hdl hdl, MessagePtr msg) {
        const std::string payload = msg->get_payload();
        std::cout << "[TA] Received registration message: " << payload << std::endl;

        // Convert UAV ID into mpz
        mpz_class uavID = str_to_mpz(payload);
        uavIDs.push_back(uavID);

        TransmissionPackage pkg;
        pkg.pp = pp;
        pkg.M = messageM;
        pkg.t = thresholdT;

        // ------------------------------------------------------------
        // Normal UAV
        // ------------------------------------------------------------
        if (payload != kUAVhID) {
            pkg.uav = getUAV(pkg.pp, poly_d, poly_b, uavID);

            // Store the PK fragment at index t-2 (required by UAVh)
            uavPKs_t.push_back(pkg.uav.PK[thresholdT - 2]);
        }

        // ------------------------------------------------------------
        // Cluster head UAVh (special) or Verifier
        // ------------------------------------------------------------
        else {
            pkg.uav.ID = uavID;
            pkg.uav.PK = uavPKs_t;   // Send all UAV PK fragments to UAVh for verifying the legitimacy of partial signatures
            pkg.uav.c1 = uavIDs;     // Provide all UAV IDs to UAVh

            // Transmission key and UAV numbers: (masterSK, N)
            pkg.uav.c2 = {alpha, kNumUAV};

            std::cout << "[TA] UAVh registered. Transformation key key α = ";
            show_mpz(alpha.get_mpz_t());
        }

        // Serialize and send response
        std::string output = Package_to_str(pkg);

        try {
            server->send(hdl, output, websocketpp::frame::opcode::text);
        }
        catch (const websocketpp::exception &e) {
            std::cerr << "[TA] Error sending message: " << e.what() << std::endl;
        }
    }


// ============================================================
// Start TA WebSocket server
// ============================================================
    int startServer() {
        Server server;

        try {
            server.set_access_channels(websocketpp::log::alevel::all);
            server.clear_access_channels(websocketpp::log::alevel::frame_payload);

            server.init_asio();

            // Bind message handler
            server.set_message_handler(
                    websocketpp::lib::bind(
                            &onRegister,
                            &server,
                            websocketpp::lib::placeholders::_1,
                            websocketpp::lib::placeholders::_2
                    )
            );

            // Bind to port 9002
            auto ip = boost::asio::ip::address::from_string("0.0.0.0");
            server.listen(boost::asio::ip::tcp::endpoint(ip, 9002));
            server.start_accept();

            std::cout << "[TA] Listening on ws://0.0.0.0:9002" << std::endl;

            // Run event loop
            server.run();
        }
        catch (const std::exception &e) {
            std::cerr << "[TA] Exception: " << e.what() << std::endl;
            return -1;
        }
        catch (...) {
            std::cerr << "[TA] Unknown exception occurred!" << std::endl;
            return -1;
        }

        return 0;
    }


// ============================================================
// Main entry
// ============================================================
    int run() {
        initParams();
        return startServer();
    }

} // namespace TA


// Standalone main (optional)
int main() {
    TA::LoadConfig("scripts/config.env");
    return TA::run();
}
