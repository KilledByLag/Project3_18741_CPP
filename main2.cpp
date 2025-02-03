#include <iostream>
#include <assert.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <thread>

using json = nlohmann::json;

class Node{
    public:
    std::filesystem::path NORMALIZED_PATH{};
    json NODE_DETAILS{};
    bool running{true};

    Node(const std::string& FILEPATH){
        NORMALIZED_PATH = normalize_path(FILEPATH);
        NODE_DETAILS = make_json_struct(NORMALIZED_PATH);
    }

    void show_node_details(){
        
        std::cout <<"HOSTNAME: "<< NODE_DETAILS["hostname"] <<std::endl;
        std::cout <<"PORT: "<< NODE_DETAILS["port"] <<std::endl;
        std::cout <<"PEERS: "<< NODE_DETAILS["peers"] <<std::endl;
        std::cout <<"CONTENT_INFO: "<< NODE_DETAILS["content_info"] <<std::endl;
        std::cout <<"PEER_INFO: "<< NODE_DETAILS["peer_info"] <<std::endl;
    }

    void input_thread(){
        std::string command{};
        while(running){

            std::cout<<"Enter a command (kill/filename):"<<std::endl;
            std::getline(std::cin, command);

                if(command!="kill"){
                    auto[exists, port] = file_exists(command);
                    std::cout<<std::boolalpha<<exists<<" "<<port<<std::endl;
                }
                else{
                    running = false;
                }
        }
    }

    private:
    std::filesystem::path normalize_path(const std::string& FILEPATH){
        std::filesystem::path p = FILEPATH;
        p = p.lexically_normal();
        return p;
    }

    json make_json_struct(std::filesystem::path FILEPATH){

         std::ifstream f(FILEPATH);
         if(!f.is_open()){
            std::cerr << "FileOpenError: Unable to open this file" <<std::endl;
            std::cout << "Make sure file exists and is not corrupted" << std::endl;
         }
         try{
            json node_details = json::parse(f);
            return node_details;
         }
         catch(std::exception& e){
            std::cerr << "JSONParseFailed: " << e.what() << std::endl;
         }
        
    }
    std::tuple<bool, int> file_exists(std::string filename){

        bool exists{false};
        int neighbor_port{};
        if(!NODE_DETAILS.contains("peer_info") || !NODE_DETAILS["peer_info"].contains("content_info")){
            return {false,0};
        }
        for(const auto& peer:NODE_DETAILS["peer_info"]){
            for(const auto& file:peer["content_info"]){
                if(filename == file){
                    exists = true;
                    neighbor_port = peer["port"];
                    return {exists,neighbor_port};
                }
            }
        }
        return {exists,neighbor_port};

    }

};

int main(int argc, char **argv){

    assert((argc == 2) && "Invalid_Args: arg[0] = filename.exe, arg[1] = FILEPATH for node)");
    Node node1(argv[1]);

    node1.show_node_details();

    //Start Input thread
    std::thread input_thread(&Node::input_thread, &node1);
    input_thread.join();


}
