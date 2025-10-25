#include "frames.h"

void PeerInfo::print_details() const {
            std::print("Hostname is {}\n", hostname);
            std::print("Port is {}\n", port);
            std::print("Content info: ");
            for (const auto& info : content_info) {
                std::print("{} ", info);
            }
            std::print("\n");
        }