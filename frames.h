#pragma once
#include <string>
#include <vector>
#include <print>


constexpr int PAYLOAD_BUFFER = 4096;
constexpr int TX_WINDOW_SIZE = 50;


struct PeerInfo{
    std::string hostname;
    int port;
    std::vector<std::string> content_info;

    void print_details() const;
  };

struct Dataframe{

    int sequence_number{};
    int payload_size;
    char data[PAYLOAD_BUFFER];
    bool end;
};

struct AckFrame{
    int ack_num{};
};