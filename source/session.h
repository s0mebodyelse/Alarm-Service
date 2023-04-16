#ifndef _H_SESSION
#define _H_SESSION

#include <cstdio>
#include <iostream>
#include <unordered_map>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <chrono>

#include "server.h"

#define MAX_CONNECTIONS 10
#define MAX_TIMERS 5

typedef struct request {
    uint32_t requestId;
    uint64_t dueTime;
    uint32_t cookieSize;
    std::string cookieData;
} request_t;

// forward declaration for server
class Server;

class Session: public std::enable_shared_from_this<Session> {
    public:
        Session(boost::asio::ip::tcp::socket socket, boost::asio::io_context &io_context, Server &server);
        ~Session();
        void open_Server(Server &server);
        void start();

        static int connections;

    private:
        void read_header();
        void read_data(uint32_t requestId);
        void respond_client(const request_t &req);
        void set_timer(const request_t &req);

        enum {
            header_length = 16,
        };

        boost::asio::steady_timer timer_;
        boost::asio::ip::tcp::socket socket_;
        Server &server_;

        // buffers, the data is then stored in the request
        std::vector<char> inbound_data_;
        std::vector<uint32_t> inbound_header_{0,0,0,0};

        // hold the data from the request, needs to be a vector or similar
        //std::vector<request_t> requests_;
        std::unordered_map<uint32_t, request_t> requests_;        
};

#endif