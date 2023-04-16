#ifndef _H_SERVER
#define _H_SERVER

#include <cstdio>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>

#include "session.h"

class Server {
    public:
        Server(boost::asio::io_context &io_context, short port);
        void open_acceptor();

    private:
        void do_accept();

        short port_;
        boost::asio::ip::tcp::endpoint endpoint_;
        boost::asio::ip::tcp::acceptor acceptor_;
        boost::asio::io_context &context_;
};

#endif