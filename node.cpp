#include "node.h"
#include <iostream>
#include <print>
#include <thread>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <arpa/inet.h>
#include <sys/socket.h>

constexpr const char* LOCAL_HOST = "127.0.0.1";

// ---------- Helper Functions ----------

bool Node::check_filepath_validity(const std::filesystem::path &path) {
    return (path.extension() == ".json");
}

bool Node::check_filepath_exists(const std::filesystem::path &path) {
    return std::filesystem::exists(path);
}

nlohmann::json Node::parse_json(const std::filesystem::path &path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + path.string());
    }
    return nlohmann::json::parse(file);
}

bool Node::create_and_bind_socket() {
    mySocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (mySocket < 0) {
        perror("socket creation failed");
        return false;
    }

    struct timeval timeout {2, 0};  // 2s timeout
    setsockopt(mySocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    sockaddr_in serverAddr{};
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, LOCAL_HOST, &serverAddr.sin_addr);
    serverAddr.sin_family = AF_INET;

    if (bind(mySocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("bind failed");
        return false;
    } else {
        std::print("\n --- Socket binding DONE --- \n");
        SocketIsBind = true;
        return true;
    }
}

int Node::find_file_in_nodes(const std::string &file) {
    for (const PeerInfo &peer : peer_info) {
        for (const std::string &peer_file : peer.content_info) {
            if (file == peer_file) {
                std::print("Found filename in: {}\n", peer.port);
                return peer.port;
            }
        }
    }
    throw std::runtime_error("Could not find file in nodes");
}

// ---------- Constructor ----------

Node::Node(const std::string &node_filepath_str) {
    node_path = std::filesystem::path(node_filepath_str);
    if (!check_filepath_validity(node_path))
        throw std::invalid_argument("Config file must be .json");

    if (!check_filepath_exists(node_path))
        throw std::invalid_argument("Config file not found");

    nlohmann::json node_data = parse_json(node_path);

    port = node_data["port"].get<int>();
    num_peers = node_data["peers"].get<int>();
    hostname = (node_data["hostname"].get<std::string>() == "localhost")
                ? LOCAL_HOST
                : node_data["hostname"].get<std::string>();
    content_info = node_data["content_info"].get<std::vector<std::string>>();

    for (const auto &peer_data : node_data["peer_info"]) {
        PeerInfo peer;
        peer.hostname = peer_data["hostname"].get<std::string>();
        peer.port = peer_data["port"].get<int>();
        peer.content_info = peer_data["content_info"].get<std::vector<std::string>>();
        peer_info.push_back(peer);
    }

    if (!create_and_bind_socket())
        throw std::runtime_error("Socket binding error");
}
// ---------- Destructor ----------
Node::~Node() {
    // Signal threads to stop if they’re still running
    kill = true;

    if (SocketIsBind) {
        close(mySocket);  // ✅ Release the socket file descriptor
        std::print("🔒 Socket on port {} closed.\n", port);
    }

    std::print("🧹 Node resources cleaned up.\n");
}

// ---------- Getters ----------

int Node::get_node_port() const { return port; }
int Node::get_node_num_peers() const { return num_peers; }
std::string Node::get_node_hostname() const { return hostname; }
std::string Node::get_node_path_str() const { return node_path.string(); }
std::vector<std::string> Node::get_node_content_info() const { return content_info; }
std::vector<PeerInfo> Node::get_node_peer_info() const { return peer_info; }

// ---------- Thread Methods ----------

void Node::take_user_input() {
    std::string user_input{};
    while (true) {
        std::cin >> user_input;
        if (user_input != "kill") {
            std::unique_lock<std::mutex> lock(user_input_queue_lock);
            user_inputs.push(user_input);
            owner = Owner::Client;
        } else if (user_input == "kill") {
            kill = true;
            break;
        } else {
            throw std::runtime_error("Invalid command by user");
        }
    }
}

void Node::start_as_server() {
    std::unordered_map<int, std::string> connections_table;
    size_t buffSize = PAYLOAD_BUFFER;
    char buffer[buffSize];
    sockaddr_in clientAddr{};
    socklen_t clientLen = sizeof(clientAddr);
    ssize_t bytesReceived{};

    std::print("Server listening on port {}...\n", port);

    while (!kill) {
        if (owner == Owner::Server) {
            {
                std::scoped_lock sockLock(socket_lock);
                bytesReceived = recvfrom(mySocket, buffer, buffSize, 0,
                                         (sockaddr *)&clientAddr, &clientLen);
            }

            if (bytesReceived > 0) {
                buffer[bytesReceived] = '\0';
                int clientPort = ntohs(clientAddr.sin_port);
                std::string msg(buffer);

                std::print("Received from client {}: {}\n", clientPort, msg);

                if (!connections_table.contains(clientPort)) {
                    connections_table[clientPort] = "";
                }

                std::string status = connections_table[clientPort];

                if (std::string_view(buffer) == ThreeWayHandshakeMessages::SYN) {
                    connections_table[clientPort] = ThreeWayHandshakeMessages::SYN;
                    sendto(mySocket, ThreeWayHandshakeMessages::SYNACK.data(),
                           ThreeWayHandshakeMessages::SYNACK.size(), 0,
                           (const sockaddr *)&clientAddr, clientLen);
                    std::print("Received SYN from client {}\n", clientPort);
                } else if (std::string_view(buffer) == ThreeWayHandshakeMessages::ACK) {
                    connections_table[clientPort] = ThreeWayHandshakeMessages::ESTABLISHED;
                    std::print("Received ACK from client {}, CONNECTION ESTABLISHED\n", clientPort);
                } else {
                    std::print("Received {}\n", msg);
                    if (connections_table[clientPort] == ThreeWayHandshakeMessages::ESTABLISHED) {
                        std::filesystem::path filepath = msg;
                        std::filesystem::path directory_path = node_path.parent_path();
                        std::filesystem::path requested_filepath = directory_path / filepath;
                        std::vector<Dataframe> dataVector = ServerUtils::create_frame_vector(requested_filepath);
                        ServerUtils::send_data(dataVector, mySocket, clientAddr, socket_lock);
                    }
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void Node::start_as_client() {
    while (!kill) {
        if (owner == Owner::Client) {
            std::unique_lock<std::mutex> lock(user_input_queue_lock);
            if (!user_inputs.empty()) {
                std::string filename = user_inputs.front();
                user_inputs.pop();
                lock.unlock();

                std::print("Finding node that contains: {}...\n", filename);
                int clientPort = find_file_in_nodes(filename);

                sockaddr_in serverAddr{};
                socklen_t serverLen = sizeof(serverAddr);
                serverAddr.sin_family = AF_INET;
                serverAddr.sin_port = htons(clientPort);
                inet_pton(AF_INET, LOCAL_HOST, &serverAddr.sin_addr);

                ClientUtils::start_rx_data_as_client(mySocket, filename, serverAddr, socket_lock, node_path);
                owner = Owner::Server;
                std::this_thread::sleep_for(std::chrono::seconds(5));
            } else {
                lock.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
    }
}

// ---------- Run Method ----------

void Node::run() {
    std::print("Node port: {}\n", port);
    std::print("Hostname: {}\n", hostname);
    std::print("Peers: {}\n", num_peers);

    std::print("Content info:\n");
    for (const auto &c : content_info) std::print(" - {}\n", c);

    std::print("Peer info:\n");
    for (size_t i = 0; i < peer_info.size(); ++i) {
        std::print("\nPeer {}:\n", i);
        peer_info[i].print_details();
    }

    std::thread server_thread(&Node::start_as_server, this);
    std::thread client_thread(&Node::start_as_client, this);
    std::thread input_thread(&Node::take_user_input, this);

    server_thread.join();
    client_thread.join();
    input_thread.join();
}
