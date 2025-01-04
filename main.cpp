#include <iostream>
#include <cassert>
#include <string>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <thread>
#include <atomic>

using json = nlohmann::json;

class Node{
    public:
        // Node file path and operating port
        std::string NODEPATH;
        int PORT;
        json DATA;
        std::atomic<bool> running{true}; // Atomic flag for threads

        //Constructor
        Node(std::string nodePath, std::string port){
        NODEPATH = parsePath(nodePath);
        PORT = parsePort(port);
        DATA = parseJSON(NODEPATH);
        }

        // Input thread : 2 commands - kill, filename
        void command_thread(){

            while (running){

                std::string command;
                std::cout << "Enter Command (Kill/filename): " << std::endl;
                std::cin >> command;

                    if (command == std::string("kill")){
                        std::cout << "Shutting down now..." << std::endl;
                        running = false;
                    }
                    else{
                        auto [found, port] = check_file_exists(command, DATA);
                        std::cout<<std::boolalpha<<found<<std::endl;
                        std::cout<<port<<std::endl;
                    }
                
            }
        }


    private:
        //Other Functions:

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
    commands.join();


    return 0;
}
