#include "server.h"

using boost::asio::ip::tcp;

Server::Server(boost::asio::io_context &io_context, short port)
        :   endpoint_(tcp::v4(), port),
            acceptor_(io_context), 
            port_(port),
            context_(io_context) 
{
    std::cout << port << std::endl;
    open_acceptor();            
}

void Server::open_acceptor(){
    boost::system::error_code errorCode;

    if (!acceptor_.is_open()) {
        acceptor_.open(endpoint_.protocol());
        // reuse port and adress
        acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
        acceptor_.bind(endpoint_);
        acceptor_.listen(boost::asio::socket_base::max_listen_connections, errorCode);
        // accept new connections
        if (!errorCode) {
            do_accept();
        }
    }
}

void Server::do_accept() { 
    acceptor_.async_accept(
        // lambda function, constructor of errorCode and socket are called automatically? 
        [this](boost::system::error_code errorCode, tcp::socket socket) {
            if (!errorCode) {
                // make a shared object, and move over the socket
                std::make_shared<Session>(std::move(socket), context_, *this) -> start();
                // at MAX_CONNECTIONS close the server acceptor
                if(Session::connections == MAX_CONNECTIONS && acceptor_.is_open()) {
                    // close the acceptor
                    acceptor_.close();
                } else {
                    do_accept();
                }
            }
        }
    );
}