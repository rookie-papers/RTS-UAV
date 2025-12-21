#include "../include/UAVh.h"

namespace UAVhNode_NS {

// ============================================================
// Global state for UAVh (Cluster Head)
// ============================================================

    gmp_randstate_t state;
    Params pp;          // system parameters from TA
    UAV_h uavh;        // cluster head's private data
    mpz_class message;     // message M
    int threshold;   // t
    int numUAV;      // n
    std::vector<parSig> partialSigs;      // collected partial signatures

    std::mutex sigMutex;
    string bitmap;

// ============================================================
// TA connection handlers (client mode)
// ============================================================

// Called when UAVh connects to TA (ws://ip:9002)
    void handleTAOpen(Client *c, connection_hdl hdl) {
        initState(state);

        std::string type = "UAVh";
        websocketpp::lib::error_code ec;

        c->send(hdl, type, websocketpp::frame::opcode::text, ec);
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
        const std::string uri = "ws://10.0.10.2:9002";

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

// Called when connection to UAV_i is opened: send the bitmap provided by Verifier
    void handleUAVOpen(Client *c, connection_hdl hdl) {
        websocketpp::lib::error_code ec;

        c->send(hdl, bitmap, websocketpp::frame::opcode::binary, ec);

        if (ec) {
            std::cerr << "[UAVh] Error sending bitmap: " << ec.message() << std::endl;
        }
        else {
            std::cout << "[UAVh] Bitmap sent to UAV." << std::endl;
        }
    }

// Handle the partial signature returned by UAV_i
    void handleUAVMessage(Client *c, connection_hdl hdl, MsgClient msg) {
        std::string payload = msg->get_payload();

        std::cout << "[UAVh] Sent aggregated signature (size: " << payload.size() << " bytes).\n";

        if (payload != "null") {
            parSig sig = str_to_parSig(payload);

            // lock
            {
                std::lock_guard<std::mutex> lock(sigMutex);
                partialSigs.push_back(sig);
            }
            std::cout << "[UAVh] Partial signature received." << std::endl;
        } else {
            std::cout << "[UAVh] UAV_i not selected in S.\n";
        }

        c->close(hdl, websocketpp::close::status::normal, "done");
    }


// Connect UAVh to each UAV_i to collect partial signatures (Parallel)
    int collectPartialSignatures() {

        std::string baseIp = "10.0.30.";
        int startIpSuffix = 101;
        std::string port = "8002";

        std::vector<std::shared_ptr<Client>> clients;
        std::vector<std::thread> threads;

        std::cout << "[UAVh] Starting PARALLEL collection..." << std::endl;

        for (int i = 0; i < numUAV; ++i) {
            auto client = std::make_shared<Client>();
            clients.push_back(client);

            std::string targetIp = baseIp + std::to_string(startIpSuffix + i);
            std::string uri = "ws://" + targetIp + ":" + port;

            threads.emplace_back([client, uri]() {
                try {
                    client->set_access_channels(websocketpp::log::alevel::none);
                    client->init_asio();

                    client->set_open_handler(
                            websocketpp::lib::bind(&handleUAVOpen, client.get(), websocketpp::lib::placeholders::_1)
                    );
                    client->set_message_handler(
                            websocketpp::lib::bind(&handleUAVMessage, client.get(),
                                                   websocketpp::lib::placeholders::_1,
                                                   websocketpp::lib::placeholders::_2)
                    );

                    websocketpp::lib::error_code ec;
                    auto con = client->get_connection(uri, ec);
                    if (ec) {
                        std::cerr << "[UAVh] Connect failed: " << uri << " - " << ec.message() << std::endl;
                        return;
                    }

                    client->connect(con);
                    client->run();
                }
                catch (const std::exception &e) {
                    std::cerr << "[UAVh Thread Exception] " << e.what() << std::endl;
                }
            });
        }

        for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }

        std::cout << "[UAVh] Collection finished. Total signatures: " << partialSigs.size() << std::endl;
        return 0;
    }


// ============================================================
// Server for Verifier (aggregated signature)
// ============================================================

    // Hex string to binary string
    std::string hexToString(const std::string& input) {
        size_t len = input.length();
        if (len & 1) throw std::invalid_argument("Odd length");
        std::string output;
        output.reserve(len / 2);
        for (size_t i = 0; i < len; i += 2) {
            std::string byteString = input.substr(i, 2);
            char byte = (char)strtol(byteString.c_str(), NULL, 16);
            output.push_back(byte);
        }
        return output;
    }

// Handle verifier request: receive "PK_v # HexBitmap"
    void handleVerifierMessage(Server *s, connection_hdl hdl, MsgServer msg) {
        std::string payload = msg->get_payload();

        size_t delPos = payload.find('#');
        if (delPos == std::string::npos) {
            std::cerr << "[UAVh] Error: Invalid payload format from Verifier." << std::endl;
            return;
        }
        std::string pkStr = payload.substr(0, delPos);
        std::string hexBitmap = payload.substr(delPos + 1);
        mpz_class PK_v = str_to_mpz(pkStr);
        bitmap = hexToString(hexBitmap);
        collectPartialSignatures();

        Sigma sigma = AggSig(partialSigs, pp, uavh, PK_v);
        std::string sigStr = Sigma_to_str(sigma);

        try {
            s->send(hdl, sigStr, websocketpp::frame::opcode::text);
            std::cout << "[UAVh] Sent aggregated signature (size: " << sigStr.size() << " bytes).\n";
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
        startUAVhServer();
        return 0;
    }

} // namespace UAVhNode


// ============================================================
// Standalone main
// ============================================================

int main() {
    return UAVhNode_NS::run();
}








/*
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
//            showParSig(sig);
    } else {
        std::cout << "[UAVh] UAV_i not selected in S.\n";
    }
    c->close(hdl, websocketpp::close::status::normal, "done");
}


// Connect UAVh to each UAV_i to collect partial signatures
int collectPartialSignatures() {
    // 适配虚拟网络环境
    std::string baseIp = "10.0.30.";
    int startIpSuffix = 101;
    std::string port = "8002"; // 所有 UAV 都监听 8002

    for (int i = 0; i < numUAV; ++i) {
        Client client;
        std::string targetIp = baseIp + std::to_string(startIpSuffix + i);
        std::string uri = "ws://" + targetIp + ":" + port;
//            std::cout << "[UAVh] Connecting to UAV at " << uri << std::endl;
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
}*/
