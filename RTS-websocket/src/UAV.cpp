#include "../include/UAV.h"

namespace UAVNode {

// ============================================================
// Global state for UAV (isolated inside namespace)
// ============================================================

    gmp_randstate_t state;
    Params          pp;      // public parameters from TA
    UAV             uav;     // UAV's private information
    mpz_class       message; // message M
    int             threshold; // threshold t


// ============================================================
// TA connection handlers (client mode)
// ============================================================

// Called when UAV successfully connects to TA (ws://ip:9002)
    void handleTAOpen(Client* c, connection_hdl hdl) {
        initState(state);

        // Generate UAV ID randomly
        uav.ID = rand_mpz(state);
        std::string idStr = mpz_to_str(uav.ID);

        websocketpp::lib::error_code ec;
        c->send(hdl, idStr, websocketpp::frame::opcode::text, ec);

        if (ec) {
            std::cerr << "[UAV] Failed to send ID to TA: " << ec.message() << std::endl;
        } else {
            std::cout << "[UAV] Sent UAV ID to TA." << std::endl;
        }
    }


// Called when TA returns UAV's key + system parameters
    void handleTAMessage(Client* c, connection_hdl hdl, MsgClient msg) {
        std::cout << "[UAV] Received parameters from TA." << std::endl;

        TransmissionPackage pkg = str_to_Package(msg->get_payload());
        pp        = pkg.pp;
        uav       = pkg.uav;
        message   = pkg.M;
        threshold = pkg.t;

        // Close the client connection after receiving TA package
        c->close(hdl, websocketpp::close::status::normal, "TA done");
    }


// ============================================================
// Connect UAV to TA (client mode)
// ============================================================

    int connectToTA() {
        Client client;

        const std::string uri = "ws://localhost:9002";

        try {
            client.set_access_channels(websocketpp::log::alevel::none);
            client.init_asio();

            // Register event handlers
            client.set_open_handler(
                    websocketpp::lib::bind(&handleTAOpen, &client, websocketpp::lib::placeholders::_1)
            );
            client.set_message_handler(
                    websocketpp::lib::bind(&handleTAMessage, &client,
                                           websocketpp::lib::placeholders::_1,
                                           websocketpp::lib::placeholders::_2)
            );

            websocketpp::lib::error_code ec;
            auto con = client.get_connection(uri, ec);
            if (ec) {
                std::cerr << "[UAV] Failed to connect to TA: " << ec.message() << std::endl;
                return -1;
            }

            client.connect(con);

            std::thread th([&client]() { client.run(); });
            if (th.joinable()) th.join();
        }
        catch (const std::exception& e) {
            std::cerr << "[UAV Exception] " << e.what() << std::endl;
            return -1;
        }

        return 0;
    }


// ============================================================
// UAV server: receives signer-set S from UAVh and returns partial signature
// ============================================================

    void serverOnMessage(Server* server, connection_hdl hdl, MsgServer msg) {
        std::string payload = msg->get_payload();
        std::cout << "[UAV] Received signer set S from UAVh: " << payload << std::endl;

        std::vector<mpz_class> signerSet = str_to_mpzArr(payload);

        // Default response: "null" (not selected)
        std::string sigStr = "null";

        // If this UAV is selected, generate partial signature
        if (std::find(signerSet.begin(), signerSet.end(), uav.ID) != signerSet.end()) {
            parSig sig = Sign(pp, uav, threshold, message, signerSet);
            sigStr = parSig_to_str(sig);

            std::cout << "[UAV] Generated partial signature." << std::endl;
            showParSig(sig);
        }

        // Send signature back to UAVh
        try {
            server->send(hdl, sigStr, websocketpp::frame::opcode::text);
        }
        catch (const websocketpp::exception& e) {
            std::cerr << "[UAV Error] Failed to send partial signature: " << e.what() << std::endl;
        }
    }


// Start UAV server (listening for UAVh)
    void startUAVServer(int port) {
        Server server;

        try {
            server.set_access_channels(websocketpp::log::alevel::none);
            server.init_asio();

            server.set_message_handler(
                    websocketpp::lib::bind(&serverOnMessage, &server,
                                           websocketpp::lib::placeholders::_1,
                                           websocketpp::lib::placeholders::_2)
            );

            server.listen(port);
            server.start_accept();

            std::cout << "[UAV] Listening on port " << port << std::endl;

            server.run();
        }
        catch (const std::exception& e) {
            std::cerr << "[UAV Server Exception] " << e.what() << std::endl;
        }
    }


// ============================================================
// Entry point for UAV process
// ============================================================

    int run(int port) {
        // Step 1: connect to TA (client)
        connectToTA();

        // Step 2: act as server and wait for UAVh
        startUAVServer(port);

        return 0;
    }

} // namespace UAVNode


// ============================================================
// Standalone main
// ============================================================
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: ./UAV <port>" << std::endl;
        return -1;
    }

    int port = std::stoi(argv[1]);
    return UAVNode::run(port);
}
