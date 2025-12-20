#include "../include/UAV.h"

namespace UAVNode_NS {

// ============================================================
// Global state for UAV (isolated inside namespace)
// ============================================================

    gmp_randstate_t state;
    Params          pp;      // public parameters from TA
    UAV             uav;     // UAV's private information
    mpz_class       message; // message M
    int             threshold; // threshold t
    vector<mpz_class> registeredIDs;


// ============================================================
// TA connection handlers (client mode)
// ============================================================

// Called when UAV successfully connects to TA (ws://ip:9002)
    void handleTAOpen(Client* c, connection_hdl hdl) {
        initState(state);

        // register to TA
        std::string type = "UAV";

        websocketpp::lib::error_code ec;
        c->send(hdl, type, websocketpp::frame::opcode::text, ec);

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
        registeredIDs = pkg.registeredIDs;

        // Close the client connection after receiving TA package
        c->close(hdl, websocketpp::close::status::normal, "TA done");
    }


// ============================================================
// Connect UAV to TA (client mode)
// ============================================================

    int connectToTA() {
        Client client;

        const std::string uri = "ws://10.0.10.2:9002";

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
        // 1. Retrieve the bitmap payload (Treat as binary data)
        std::string payload = msg->get_payload();
        std::string sigStr = "null";

        // 2. Retrieve local serial number (assumed range: 0 to n-1)
        int myIndex = uav.serialNumber;

        // 3. Check if the current UAV is selected in the bitmap
        bool isSelected = false;
        int myByteIndex = myIndex / 8;
        int myBitIndex  = myIndex % 8;

        // Safety check: Ensure the received bitmap is large enough to contain this index
        if (myByteIndex < payload.size()) {
            // Extract byte -> Shift -> Check if the specific bit is 1
            uint8_t byteVal = static_cast<uint8_t>(payload[myByteIndex]);
            isSelected = (byteVal >> myBitIndex) & 1;
        }

        // 4. If selected, proceed with signing operations
        if (isSelected) {
            std::vector<mpz_class> signerSet;
            // Iterate through the entire bitmap to identify all selected UAV IDs
            for (int i = 0; i < pp.n; ++i) {
                int byteIdx = i / 8;
                int bitIdx  = i % 8;

                if (byteIdx < payload.size()) {
                    uint8_t val = static_cast<uint8_t>(payload[byteIdx]);
                    if ((val >> bitIdx) & 1) {
                        signerSet.push_back(registeredIDs[i]);
                    }
                }
            }

            // 5. Execute Signing
            parSig sig = Sign(pp, uav, threshold, message, signerSet);
            sigStr = parSig_to_str(sig);

            std::cout << "[UAV] Generated partial signature." << std::endl;
            // showParSig(sig);
        } else {
            std::cout << "[UAV] Not selected. Idle." << std::endl;
        }

        // 6. Send response back to UAVh
        try {
            server->send(hdl, sigStr, websocketpp::frame::opcode::text);
        }
        catch (const websocketpp::exception& e) {
            std::cerr << "[UAV Error] Failed to send partial signature: " << e.what() << std::endl;
        }
    }


// Start UAV server (listening for UAVh)
    void startUAVServer() {
        Server server;
        int port = 8002;

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

    int run() {
        // Step 1: connect to TA (client)
        connectToTA();

        // Step 2: act as server and wait for UAVh
        startUAVServer();

        return 0;
    }

} // namespace UAVNode


// ============================================================
// Standalone main
// ============================================================
int main() {
    return UAVNode_NS::run();
}
