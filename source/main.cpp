#include <iostream>
#include <boost/asio.hpp>

#include "server.h"

int main(int argc, const char *argv[]) {
    
    short port;

    if (argc != 2) {
        std::cerr << "no port given" << std::endl;
        exit(EXIT_FAILURE);
    }

    // read the port from command line
    port = atoi(argv[1]);

    try {
        boost::asio::io_context io_context;
        Server serverInstance(io_context, port);
        io_context.run();
    } catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << "\n";
        exit(EXIT_FAILURE);
    }

    return 0;
}
