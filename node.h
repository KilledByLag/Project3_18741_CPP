#pragma once
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <atomic>
#include <filesystem>
#include <unordered_map>
#include <netinet/in.h>
#include "frames.h"
#include "network_utils.h"
#include <nlohmann/json.hpp>

class Node {
private:
    std::filesystem::path node_path;
    std::string hostname;
    std::vector<std::string> content_info;
    std::vector<PeerInfo> peer_info;

    std::queue<std::string> user_inputs;
    std::mutex user_input_queue_lock;
    std::mutex socket_lock;
    std::atomic<Owner> owner = Owner::Server;

    int port = -1;
    int num_peers = -1;
    int num_connection_queue = 10;

    int mySocket{};
    bool SocketIsBind = false;
    std::atomic<bool> kill = false;

    // private helpers
    bool check_filepath_validity(const std::filesystem::path &path);
    bool check_filepath_exists(const std::filesystem::path &path);
    nlohmann::json parse_json(const std::filesystem::path &path);
    bool create_and_bind_socket();
    int find_file_in_nodes(const std::string &file);

public:
    explicit Node(const std::string &node_filepath_str);
    ~Node(); 
    // getters
    int get_node_port() const;
    int get_node_num_peers() const;
    std::string get_node_hostname() const;
    std::string get_node_path_str() const;
    std::vector<std::string> get_node_content_info() const;
    std::vector<PeerInfo> get_node_peer_info() const;

    // threads
    void take_user_input();
    void start_as_server();
    void start_as_client();

    // convenience: main control function
    void run();
};
