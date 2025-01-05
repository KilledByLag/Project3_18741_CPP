#include <iostream>
#include <cassert>
#include <string>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <thread>
#include <atomic>
#include <winsock2.h>
#include <mutex>

#pragma comment(lib, "ws2_32.lib") // Link Winsock library



using json = nlohmann::json;

class Node{
    public:
        // Node file path and operating port
        std::string NODEPATH;
        int PORT;
        json DATA;
        std::atomic<bool> running{true}; // Atomic flag for threads
        std::string HOST = "127.0.0.1";
        SOCKET node_socket;

        std::mutex data_mutex; // Lock for shared resources
        //The shared resource
        int shared_port{};
        std::string shared_filename{};
        bool data_ready{false};

        //Constructor
        Node(std::string nodePath, std::string port){
        NODEPATH = parsePath(nodePath);
        PORT = parsePort(port);
        DATA = parseJSON(NODEPATH);
        initializewinsock();
        node_socket = initializeSocket();
        }
        //Destructor
        ~Node(){
            WSACleanup();
        }

        // Input thread : 2 commands - kill, filename
        void command_thread(){

            while (running){

                std::string command;
                std::cout << "Enter Command (Kill/filename): " << std::endl;
                std::cin >> command;

                    if (command =="kill"){
                        std::cout << "Shutting down now..." << std::endl;
                        running = false;
                    }
                    else{
                        auto [found, port] = check_file_exists(command, DATA);
                        if (found){
                            std::cout<<std::boolalpha<<found<<std::endl;
                            std::cout<<port<<std::endl;
                            {
                                std::lock_guard<std::mutex> lock(data_mutex);
                                shared_filename = command;
                                shared_port = port;
                                data_ready = true;
                            }
                        }
                        else{
                            std::cout<<"File not found"<<std::endl;
                        }


                    }
                
            }
        }

        void client_thread(){
            //My address
            sockaddr_in client_addr;
            client_addr.sin_family = AF_INET;
            client_addr.sin_port = htons(PORT);
            client_addr.sin_addr.s_addr = inet_addr(HOST.c_str());

            sockaddr_in server_addr;
            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = inet_addr(HOST.c_str());

            std::string filename{};

            if(bind(node_socket, (sockaddr*)&client_addr, sizeof(client_addr)) == SOCKET_ERROR){
                std::cerr << "Error binding socket" << std::endl;
                running = false;
            }
            while(running){
                {
                    std::lock_guard<std::mutex> lock(data_mutex);
                    if (shared_port != 0){
                            server_addr.sin_port = htons(shared_port);
                            filename = shared_filename;
                            
                            int result = sendto(node_socket, filename.c_str(), static_cast<int>(filename.size()),0, (sockaddr*)(&server_addr),sizeof(server_addr));

                            if (result == SOCKET_ERROR){
                                std::cerr << "Failed to send filename" << WSAGetLastError() << std::endl;
                            }
                            else{
                                std::cout<< "Filename sent to " << shared_port << " Successfully" <<std::endl;
                                shared_port = 0;
                                shared_filename = "";
                            }
                    }
\
                }
            }
        }
            
    private:
        //Other Functions:

        WSADATA wsa_data; //data about winsock implementation
        // Initialize winsock
        void initializewinsock(){
            int wsa_result = WSAStartup(MAKEWORD(2,2), &wsa_data);
            if (wsa_result != 0){
                std::cerr << "Winsock initialization failed" << std::endl;
                running = false;
            }
            else{
                std::cout << "Winsock initialization success" <<std::endl;
            }
        }
        // Initialize socket
        SOCKET initializeSocket(){
            SOCKET node_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (node_socket == INVALID_SOCKET){
                std::cerr << "Error creating UDP Socket" << std::endl;
                running = false;
            }
            else{
                std::cout << "Creating socket successful" << std::endl;
            }
            return node_socket;

        }

        //Parse Path
        std::string parsePath(std::string path){
            return std::filesystem::weakly_canonical(std::filesystem::path(path)).string();
        }
        //Parse Port
        int parsePort(std::string port){
            return std::stoi(port);
        }
        //Read JSON File
        json parseJSON(std::string path){
            std::ifstream file(path);
            return json::parse(file);
        }
        //Check whether requested file exists in peer_content
        std::tuple<bool, int> check_file_exists(std::string filename, json& DATA){
            bool found{false};
            int neighbor_port{};
            for(const auto& peer:DATA["peer_info"]){
                for(const std::string& file:peer["content_info"]){
                    if (filename == file){
                        found = true;
                        neighbor_port = peer["port"];
                        return {found, neighbor_port};
                    }
                }

            }
            return {found, neighbor_port};
        }


};


int main(int argc, char **argv){

    // Check if num_args is valid
    assert((argc == 3) && "Invalid Args ----- args[0] = main.exe, args[1] == {node_filepath}, args[2] == {port_num}");

    //Input the args to the Node Class

    Node node(argv[1], argv[2]); //Used the constructor in the class

    std::cout << "the NODEPATH is: " << node.NODEPATH << std::endl;
    std::cout<< "The operating PORT is: " << node.PORT << std::endl;
    std::cout << "The node data is: " << node.DATA << std::endl;

    //Start threads here
    std::thread commands(&Node::command_thread, &node); //Non static member, so operates on the instance of the class, therefore need to pass the reference to the func, reference to the class instance
    std::thread client(&Node::client_thread,&node);

    commands.join();
    client.join();


    return 0;
}
