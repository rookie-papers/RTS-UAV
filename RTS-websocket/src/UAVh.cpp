#include "../include/UAVh.h"


namespace UAVhNode {

// ============================================================
// Global state for UAVh (Cluster Head)
// ============================================================

    gmp_randstate_t state;
    Params pp;          // system parameters from TA
    UAV_h uavh;        // cluster head's private data
    mpz_class message;     // message M
    int threshold;   // t
    int numUAV;      // n
    std::vector<mpz_class> registeredIDs;      // all UAV IDs in cluster
    std::vector<parSig> partialSigs;      // collected partial signatures


// ============================================================
// TA connection handlers (client mode)
// ============================================================

// Called when UAVh connects to TA (ws://ip:9002)
    void handleTAOpen(Client *c, connection_hdl hdl) {
        initState(state);

        // Fixed aggregator ID for testing/demo
        uavh.ID = 0x6666666666666666666666666666666666666666666666666666666666666666_mpz;

        std::string idStr = mpz_to_str(uavh.ID);
        websocketpp::lib::error_code ec;

        c->send(hdl, idStr, websocketpp::frame::opcode::text, ec);
        if (ec) {
            std::cerr << "[UAVh] Failed to send ID to TA: " << ec.message() << std::endl;
        } else {
            std::cout << "[UAVh] Aggregator ID sent to TA." << std::endl;
        }
    }


// Called when TA replies with parameters and UAVh secret
    void handleTAMessage(Client *c, connection_hdl hdl, MsgClient msg) {
        std::cout << "[UAVh] Received registration package from TA." << std::endl;

        TransmissionPackage pkg = str_to_Package(msg->get_payload());

        pp = pkg.pp;
        uavh.ID = pkg.uav.ID;
        uavh.alpha = pkg.uav.c2[0];
        registeredIDs = pkg.uav.c1;  // all UAV_i IDs
        message = pkg.M;
        threshold = pkg.t;
        numUAV = pkg.uav.c2[1].get_si();

        std::cout << "[UAVh] Alpha received = ";
        show_mpz(uavh.alpha.get_mpz_t());

        // Close connection after loading parameters
        c->close(hdl, websocketpp::close::status::normal, "TA done");
    }


// ============================================================
// Connect UAVh to TA and receive parameters
// ============================================================

    int connectToTA() {
        Client client;
        const std::string uri = "ws://localhost:9002";

        try {
            client.set_access_channels(websocketpp::log::alevel::none);
            client.init_asio();

            client.set_open_handler(
                    websocketpp::lib::bind(&handleTAOpen, &client, websocketpp::lib::placeholders::_1)
            );
            client.set_message_handler(
                    websocketpp::lib::bind(
                            &handleTAMessage,
                            &client,
                            websocketpp::lib::placeholders::_1,
                            websocketpp::lib::placeholders::_2
                    )
            );

            websocketpp::lib::error_code ec;
            auto con = client.get_connection(uri, ec);
            if (ec) {
                std::cerr << "[UAVh] Failed to connect to TA: " << ec.message() << std::endl;
                return -1;
            }

            client.connect(con);

            std::thread t([&client]() { client.run(); });
            if (t.joinable()) t.join();
        }
        catch (const std::exception &e) {
            std::cerr << "[UAVh Exception] " << e.what() << std::endl;
            return -1;
        }

        return 0;
    }

// ============================================================
// Collect partial signatures from UAV_i
// ============================================================

// Called when connection to UAV_i is opened: send signer set S
    void handleUAVOpen(Client *c, connection_hdl hdl) {
        // Select the first t UAVs as signer set S
        std::vector<mpz_class> S(registeredIDs.begin(),
                                 registeredIDs.begin() + threshold);

        std::string str = mpzArr_to_str(S);

        websocketpp::lib::error_code ec;
        c->send(hdl, str, websocketpp::frame::opcode::text, ec);

        if (ec) {
            std::cerr << "[UAVh] Failed to send S to UAV_i: "
                      << ec.message() << std::endl;
        }
    }


// Handle the partial signature returned by UAV_i
    void handleUAVMessage(Client *c, connection_hdl hdl, MsgClient msg) {
        std::string payload = msg->get_payload();

        if (payload != "null") {
            parSig sig = str_to_parSig(payload);
            partialSigs.push_back(sig);

            std::cout << "[UAVh] Partial signature received." << std::endl;
            showParSig(sig);
        } else {
            std::cout << "[UAVh] UAV_i not selected in S.\n";
        }

        c->close(hdl, websocketpp::close::status::normal, "done");
    }


// Connect UAVh to each UAV_i to collect partial signatures
    int collectPartialSignatures() {
        std::string ip = "ws://localhost";
        int basePort = 8002;

        for (int i = 0; i < numUAV; ++i) {
            Client client;
            std::string uri = ip + ":" + std::to_string(basePort + i);

            std::cout << "[UAVh] Connecting to UAV at " << uri << std::endl;

            try {
                client.set_access_channels(websocketpp::log::alevel::none);
                client.init_asio();

                client.set_open_handler(
                        websocketpp::lib::bind(&handleUAVOpen, &client, websocketpp::lib::placeholders::_1)
                );
                client.set_message_handler(
                        websocketpp::lib::bind(&handleUAVMessage, &client,
                                               websocketpp::lib::placeholders::_1,
                                               websocketpp::lib::placeholders::_2)
                );

                websocketpp::lib::error_code ec;
                auto con = client.get_connection(uri, ec);
                if (ec) {
                    std::cerr << "[UAVh] Failed to connect to UAV_i: "
                              << ec.message() << std::endl;
                    continue;
                }

                client.connect(con);

                std::thread t([&client]() { client.run(); });
                if (t.joinable()) t.join();
            }
            catch (const std::exception &e) {
                std::cerr << "[UAVh Exception] " << e.what() << std::endl;
            }
        }

        return 0;
    }


// ============================================================
// Server for Verifier (aggregated signature)
// ============================================================

// Handle verifier request: receive PK_v and reply with aggregated signature
    void handleVerifierMessage(Server *s, connection_hdl hdl, MsgServer msg) {
        std::string payload = msg->get_payload();
        mpz_class PK_v = str_to_mpz(payload);

        std::cout << "[Verifier] PK_v received.\n";

        Sigma sigma = AggSig(partialSigs, pp, uavh, PK_v);
        std::string sigStr = Sigma_to_str(sigma);

        try {
            s->send(hdl, sigStr, websocketpp::frame::opcode::text);
        }
        catch (const websocketpp::exception &e) {
            std::cerr << "[UAVh] Failed to send aggregated signature: "
                      << e.what() << std::endl;
        }
    }


// Start server for verifier (port 8001)
    void startUAVhServer() {
        Server server;

        try {
            server.set_access_channels(websocketpp::log::alevel::none);
            server.init_asio();

            server.set_message_handler(
                    websocketpp::lib::bind(&handleVerifierMessage,
                                           &server,
                                           websocketpp::lib::placeholders::_1,
                                           websocketpp::lib::placeholders::_2)
            );

            server.listen(8001);
            server.start_accept();

            std::cout << "[UAVh] Server running on port 8001." << std::endl;
            server.run();
        }
        catch (const std::exception &e) {
            std::cerr << "[UAVh Server Exception] " << e.what() << std::endl;
        }
    }


// ============================================================
// Entry point for UAVh process
// ============================================================

    int run() {
        connectToTA();
        collectPartialSignatures();
        startUAVhServer();
        return 0;
    }

} // namespace UAVhNode


// ============================================================
// Standalone main
// ============================================================

int main() {
    return UAVhNode::run();
}