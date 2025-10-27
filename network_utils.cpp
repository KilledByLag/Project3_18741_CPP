#include "network_utils.h"
#include <print>
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <thread>
#include <chrono>
#include <algorithm> // for std::sort

// ---------- ClientUtils Implementation ----------

bool ClientUtils::start_handshake(const int &rx_socket, sockaddr_in &serverAddr) {
    std::print("[Client] Starting 3-way handshake with server {}...\n", ntohs(serverAddr.sin_port));
    char buffer[1024];
    socklen_t serverLen = sizeof(serverAddr);
    ssize_t bytesReceived;

    sendto(rx_socket, ThreeWayHandshakeMessages::SYN.data(),
           ThreeWayHandshakeMessages::SYN.size(), 0,
           (const sockaddr *)&serverAddr, sizeof(serverAddr));
    std::print("[Client] Sent SYN\n");

    bytesReceived = recvfrom(rx_socket, buffer, sizeof(buffer) - 1, 0,
                             (sockaddr *)&serverAddr, &serverLen);
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        std::string_view received_msg(buffer);
        std::print("[Client] Received during handshake: {}\n", received_msg);
        if (received_msg == ThreeWayHandshakeMessages::SYNACK) {
            sendto(rx_socket, ThreeWayHandshakeMessages::ACK.data(),
                   ThreeWayHandshakeMessages::ACK.size(), 0,
                   (const sockaddr *)&serverAddr, sizeof(serverAddr));
            std::print("[Client] Connection established with: {}\n",
                       ntohs(serverAddr.sin_port));
            return true;
        } else {
            throw std::runtime_error("Handshake failed, unexpected response");
        }
    }
    throw std::runtime_error("Handshake failed: no response");
    return false;
}


void ClientUtils::start_rx_data_as_client(const int &rx_socket,
                                          const std::string &filename,
                                          sockaddr_in &serverAddr,
                                          std::mutex &socket_lock,
                                          const std::filesystem::path &node_path) {
    std::print("📡 [Client] Preparing to receive file '{}'...\n", filename);
    char recvBuffer[sizeof(Dataframe)];
    bool handshakeStatus = false;
    socklen_t serverLen = sizeof(serverAddr);
    std::vector<Dataframe> rx_frames;
    bool isEOF = false;
    char ackTxBuffer[sizeof(AckFrame)];

    {
        std::scoped_lock sockLock(socket_lock);
        handshakeStatus = start_handshake(rx_socket, serverAddr);
    }

    if (!handshakeStatus)
        throw std::runtime_error("Handshake failed");

    // Send filename request
    {
        std::scoped_lock sockLock(socket_lock);
        sendto(rx_socket, filename.c_str(), filename.size(), 0,
               (const sockaddr *)&serverAddr, serverLen);
        std::print("📤 [Client] Requested file: {}\n", filename);
    }

    size_t frames_received = 0;
    int expected_seq = 0;
    int last_ack_sent = -1;
    AckFrame ack{};
    ssize_t bytesReceived;
    std::print("[Client] Waiting for frames...\n");

    while (!isEOF) {
        {
            std::scoped_lock sock_lock(socket_lock);
            bytesReceived = recvfrom(rx_socket, recvBuffer, sizeof(recvBuffer), 0,
                                     (sockaddr *)&serverAddr, &serverLen);
        }

        if (bytesReceived < 0) {
            std::print("[Client] No data received (retrying...)\n");
            continue;
        }

        Dataframe rx_frame{};
        std::memcpy(&rx_frame, recvBuffer, sizeof(Dataframe));

        if (rx_frame.sequence_number == expected_seq) {
            rx_frames.push_back(rx_frame);
            frames_received++;
            expected_seq++;
            isEOF = rx_frame.end;

            ack.ack_num = rx_frame.sequence_number;
            std::memset(ackTxBuffer, 0, sizeof(ackTxBuffer));
            std::memcpy(ackTxBuffer, &ack, sizeof(ack));
            sendto(rx_socket, ackTxBuffer, sizeof(ackTxBuffer), 0,
                   (const sockaddr *)&serverAddr, serverLen);

            last_ack_sent = ack.ack_num;
            std::print("[Client] Received frame {}, sent CACK {}\n",
                       rx_frame.sequence_number, ack.ack_num);
        } else {
            std::print("[Client] Out-of-order frame {} (expected {}), resending CACK {}\n",
                       rx_frame.sequence_number, expected_seq, last_ack_sent);

            ack.ack_num = last_ack_sent;
            std::memset(ackTxBuffer, 0, sizeof(ackTxBuffer));
            std::memcpy(ackTxBuffer, &ack, sizeof(ack));
            sendto(rx_socket, ackTxBuffer, sizeof(ackTxBuffer), 0,
                   (const sockaddr *)&serverAddr, serverLen);
        }
    }

    std::print("💾 [Client] All frames received, writing file...\n");

    std::sort(rx_frames.begin(), rx_frames.end(),
              [](const Dataframe &a, const Dataframe &b) {
                  return a.sequence_number < b.sequence_number;
              });

    std::string outname = "received_" + filename;
    std::filesystem::path filepath = outname;
    std::filesystem::path directory_path = node_path.parent_path();
    std::filesystem::path requested_filepath = directory_path / filepath;

    std::ofstream outfile(requested_filepath, std::ios::binary);
    if (!outfile)
        throw std::runtime_error("Failed to open output file: " + outname);

    for (const Dataframe &frame : rx_frames) {
        outfile.write(frame.data, frame.payload_size);
    }

    outfile.close();
    std::print("[Client] File '{}' received successfully ({} frames)\n",
               outname, frames_received);
}


// ---------- ServerUtils Implementation ----------

std::vector<Dataframe> ServerUtils::create_frame_vector(const std::filesystem::path &filepath) {
    size_t bufsize = PAYLOAD_BUFFER;
    std::ifstream file(filepath, std::ios::binary);
    std::vector<Dataframe> frames;

    if (!file)
        throw std::runtime_error("Could not read file: " + filepath.string());

    size_t size_of_file = std::filesystem::file_size(filepath);
    size_t bytes_framed = 0;
    int seq_num = 0;
    char buffer[PAYLOAD_BUFFER];

    std::print("[Server] Framing file: '{}' ({} bytes)\n",
               filepath.filename().string(), size_of_file);

    while (bytes_framed < size_of_file) {
        std::memset(buffer, 0, bufsize);
        size_t chunk = std::min(bufsize, size_of_file - bytes_framed);

        Dataframe txdata{};
        txdata.sequence_number = seq_num;
        txdata.payload_size = chunk;
        txdata.end = (chunk < bufsize);

        file.read(buffer, chunk);
        std::memcpy(&txdata.data, buffer, chunk);
        frames.push_back(txdata);

        bytes_framed += chunk;
        seq_num++;
    }

    std::print("[Server] Created {} frames from '{}'\n",
               frames.size(), filepath.filename().string());
    return frames;
}


void ServerUtils::send_data(std::vector<Dataframe> &frames,
                            int mySocket,
                            sockaddr_in &clientAddr,
                            std::mutex &socket_lock) {
    const size_t total_frame_size = sizeof(Dataframe);
    const size_t total_ack_size = sizeof(AckFrame);
    char txbuffer[total_frame_size];
    char rxbuffer[total_ack_size];

    int tx_window_size = TX_WINDOW_SIZE;
    int seq_num_base = 0;
    int seq_num_next = 0;
    int seq_num_max = frames.size();
    size_t clientLen = sizeof(clientAddr);

    const auto timeout_total = std::chrono::milliseconds(500);

    std::print("[Server] Sending {} frames to client {}...\n",
               frames.size(), ntohs(clientAddr.sin_port));

    while (seq_num_base < seq_num_max) {
        // Send frames within window
        while (seq_num_next < seq_num_base + tx_window_size && seq_num_next < seq_num_max) {
            const auto &frame = frames[seq_num_next];
            std::memset(txbuffer, 0, total_frame_size);
            std::memcpy(txbuffer, &frame, sizeof(Dataframe));

            {
                std::scoped_lock sock_lock(socket_lock);
                sendto(mySocket, txbuffer, sizeof(txbuffer), 0,
                       (const sockaddr *)&clientAddr, static_cast<socklen_t>(clientLen));
            }

            std::print("[Server] Sent frame {}\n", frame.sequence_number);
            seq_num_next++;
        }

        auto time_start = std::chrono::steady_clock::now();
        ssize_t bytesReceived;
        AckFrame ack{};
        bool ack_received = false;

        while (std::chrono::steady_clock::now() - time_start < timeout_total) {
            bytesReceived = recvfrom(mySocket, rxbuffer, total_ack_size, MSG_DONTWAIT,
                                     (sockaddr *)&clientAddr, (socklen_t *)&clientLen);
            if (bytesReceived > 0) {
                std::memcpy(&ack, rxbuffer, sizeof(AckFrame));
                if (ack.ack_num >= seq_num_base && ack.ack_num < seq_num_max) {
                    std::print("[Server] CACK {} received — sliding base {} → {}\n",
                               ack.ack_num, seq_num_base, ack.ack_num + 1);
                    seq_num_base = ack.ack_num + 1;
                    ack_received = true;
                    if (seq_num_base == seq_num_max)
                        break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
        }

        if (!ack_received) {
            std::print("⏳ [Server] Timeout — resending window from {} to {}\n",
                       seq_num_base,
                       std::min(seq_num_base + tx_window_size - 1, seq_num_max - 1));

            for (int i = seq_num_base; i < seq_num_next && i < seq_num_max; ++i) {
                const auto &frame = frames[i];
                std::memset(txbuffer, 0, total_frame_size);
                std::memcpy(txbuffer, &frame, sizeof(Dataframe));

                {
                    std::scoped_lock sock_lock(socket_lock);
                    sendto(mySocket, txbuffer, sizeof(txbuffer), 0,
                           (const sockaddr *)&clientAddr, static_cast<socklen_t>(clientLen));
                }

                std::print("[Server] Resent frame {}\n", frame.sequence_number);
            }
        }
    }
    std::print("[Server] Completed sending {} frames successfully!\n", seq_num_max);
}
