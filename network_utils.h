#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <mutex>
#include <netinet/in.h>  // for sockaddr_in
#include "frames.h"      // we use Dataframe, AckFrame, etc.

enum class Owner { Server, Client };

struct ThreeWayHandshakeMessages {
    static constexpr std::string_view SYN          = "SYN";
    static constexpr std::string_view SYNACK       = "SYNACK";
    static constexpr std::string_view ACK          = "ACK";
    static constexpr std::string_view ESTABLISHED  = "ESTABLISHED";
};

struct ClientUtils {
    static bool start_handshake(const int &rx_socket, sockaddr_in &serverAddr);
    static void start_rx_data_as_client(const int &rx_socket,
                                        const std::string &filename,
                                        sockaddr_in &serverAddr,
                                        std::mutex &socket_lock,
                                        const std::filesystem::path &node_path);
};

struct ServerUtils {
    static std::vector<Dataframe> create_frame_vector(const std::filesystem::path &filepath);
    static void send_data(std::vector<Dataframe> &frames,
                          int mySocket,
                          sockaddr_in &clientAddr,
                          std::mutex &socket_lock);
};
