#include "node.h"
#include <iostream>

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <node_name.json>\n";
        return 1;
    }

    try {
        Node node(argv[1]);
        node.run();
    } catch (const std::invalid_argument &ex) {
        std::cerr << "Invalid argument: " << ex.what() << '\n';
        return 2;
    } catch (const std::runtime_error &ex) {
        std::cerr << "Runtime error: " << ex.what() << '\n';
        return 3;
    }
}
