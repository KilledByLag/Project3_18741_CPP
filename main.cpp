#include <iostream>
#include <cassert>
#include <string>
#include <filesystem>



class Node{
    public:

        // Node file path and operating port
        std::string NODEPATH;
        int PORT;

        //Make Constructor for feeding the path and port
        Node(std::string nodePath, std::string port){
        NODEPATH = parsePath(nodePath);
        PORT = parsePort(port);
        }

    
    private:
        // Parse Path
        std::string parsePath(std::string path){
            return std::filesystem::weakly_canonical(std::filesystem::path(path)).string();
        }
        //Parse Port
        int parsePort(std::string port){
            return std::stoi(port);
        }
};


int main(int argc, char **argv){

    // Check if num_args is valid
    assert((argc == 3) && "Invalid Args ----- args[0] = main.exe, args[1] == {node_filepath}, args[2] == {port_num}");

    //Input the args to the Node Class

    Node node(argv[1], argv[2]); //Used the constructor in the class

    std::cout << "the NODEPATH is: " << node.NODEPATH << std::endl;
    std::cout<< "The operating PORT is: " << node.PORT << std::endl;

    //Start threads here


    return 0;
}
