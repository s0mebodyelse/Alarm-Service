#include "session.h"

using boost::asio::ip::tcp;

int Session::connections = 0;

// the socket gets moved into the session
Session::Session(tcp::socket socket, boost::asio::io_context &io_context, Server &server)
        :   socket_(std::move(socket)), 
            timer_(io_context),
            server_(server)
{       
    // count the active connections
    connections++;
}

Session::~Session(){
    connections--;

    if (connections < MAX_CONNECTIONS) {
        open_Server(server_);
    }
}

void Session::start() {
    // check if the client has more then max timers runnning
    if (requests_.size() >= MAX_TIMERS) {
        // error
        // close the connection?
        std::cerr << "to many timers running" << std::endl;
    }
    
    // start by reading the header to get the message length
    read_header();  
}

void Session::open_Server(Server &server){
    server.open_acceptor();
}

void Session::read_header(){
    auto self(shared_from_this());
    boost::asio::async_read(socket_, boost::asio::buffer(&inbound_header_.front(), header_length), 
        [this, self](boost::system::error_code ec, std::size_t length){
            if (!ec) {
                // create a new request
                std::cout << "Request incoming" << std::endl;
                request_t req;

                // read the header into request struct
                req.requestId = ntohl(inbound_header_[0]);
                
                uint32_t high = ntohl(inbound_header_[1]);
                uint32_t low = ntohl(inbound_header_[2]);
                req.dueTime = (((uint64_t) low) | ((uint64_t) high) << 32);
                req.cookieSize = ntohl(inbound_header_[3]);

                //requests_.push_back(req);

                if (requests_.contains(req.requestId)) {
                    // error
                    std::cerr << "Error: there already is a running request with id: " << req.requestId << std::endl;
                    // send response to client? close connection?
                }

                requests_[req.requestId] = req;

                //resize the inbound data vector so the cookiedata can fit
                inbound_data_.resize(req.cookieSize);

                std::cout << "New Request: " << req.requestId << " " << req.dueTime << " " << req.cookieSize << std::endl; 

                // start reading the cookiedata
                read_data(req.requestId);
            } else {
                std::cerr << "Error: async read: " << ec.message() << std::endl;
            }
        }
    );
}

void Session::read_data(uint32_t requestId){           
    auto self(shared_from_this());
    boost::asio::async_read(socket_, boost::asio::buffer(inbound_data_), 
        [this, self, requestId](boost::system::error_code ec, std::size_t length){
            if (!ec) {
                std::string cookieData(&inbound_data_[0], inbound_data_.size());

                std::cout << "Client send: " << cookieData << " in request with id: " << requestId<< std::endl;

                requests_[requestId].cookieData = cookieData;

                set_timer(requests_[requestId]);
            } else {
                std::cerr << "error reading data " << ec.message() << std::endl;
            }
        }
    );
}

void Session::respond_client(const request_t &req){            

    // build buffer
    std::vector<boost::asio::const_buffer> buffer;
    buffer.push_back(boost::asio::buffer(&req.requestId, sizeof(uint32_t)));
    buffer.push_back(boost::asio::buffer(&req.cookieSize, sizeof(uint32_t)));
    // in case of cookieData, the buffer can be directly build from std::string
    buffer.push_back(boost::asio::buffer(req.cookieData, req.cookieSize));

    // send the buffer to the client
    boost::asio::async_write(socket_, buffer,
        [this](boost::system::error_code ec, std::size_t length){
            if (!ec) {
                // if everything is going alright, we print the len of the send message
                std::cout << "send len: " << length << std::endl;                        
            } else {
                std::cerr << "error responding client: " << ec.message() << std::endl;
            }
        }
    );

    // Delete the request from the requests vector
    requests_.erase(req.requestId);
}

void Session::set_timer(const request_t &req){ 
    // keep the session alive during the wait operation
    auto self(shared_from_this());
    
    // set the timer to wait for 1 second
    timer_.expires_after(boost::asio::chrono::seconds(1));
    
    // start waiting
    timer_.async_wait(
        [this, self, req](boost::system::error_code ec){
            if (!ec) {
                // get system time
                using namespace std::chrono;
                uint64_t sysTime = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();

                if (sysTime >= req.dueTime) {
                    // time is up, respond to client
                    std::cout << "time is up" << std::endl;
                    //respond to client
                    respond_client(req);
                } else {
                    //keep on waiting
                    std::cout << "ID: " << req.requestId << " Waiting..." << std::endl;
                    set_timer(req);
                }
            } else {
                std::cerr << "error setting timer: " << ec.message() << " " << ec.value() << std::endl;
            }
        }
    );
}