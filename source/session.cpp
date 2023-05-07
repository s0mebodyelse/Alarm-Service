#include "session.h"

using boost::asio::ip::tcp;

int Session::connections = 0;

Session::Session(tcp::socket socket, boost::asio::io_context &io_context, Server &server)
        :   socket_(std::move(socket)), 
            timer_(io_context),
            server_(server),
            context_(io_context)
{       
    connections++;
}

Session::~Session(){
    connections--;

    if (connections < MAX_CONNECTIONS) {
        open_Server(server_);
    }
}

void Session::open_Server(Server &server){
    server.open_acceptor();
}

void Session::start() {
    if (requests_.size() >= MAX_TIMERS) {
        std::cerr << "to many timers running" << std::endl;
        return;
    }
    
    read_header();  
}

void Session::read_header(){
    auto self(shared_from_this());
    boost::asio::async_read(socket_, boost::asio::buffer(&inbound_header_.front(), header_length), 
        [this, self](boost::system::error_code ec, std::size_t length){
            if (!ec) {
                std::cout << "New Request incoming" << std::endl;
                request_t req;

                // read the header into request struct
                req.requestId = ntohl(inbound_header_[0]);
                uint32_t high = ntohl(inbound_header_[1]);
                uint32_t low = ntohl(inbound_header_[2]);
                req.dueTime = (((uint64_t) low) | ((uint64_t) high) << 32);
                req.cookieSize = ntohl(inbound_header_[3]);

                if (requests_.contains(req.requestId)) {
                    std::cerr << "Error: there already is a running request with id: " << req.requestId << std::endl;
                }

                requests_[req.requestId] = req;

                inbound_data_.resize(req.cookieSize);

                std::cout << "New Request: " << req.requestId << " " << req.dueTime << " " << req.cookieSize << std::endl; 

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
                read_header();
            } else {
                std::cerr << "error reading data " << ec.message() << std::endl;
            }
        }
    );
}

void Session::write_message(){            
    request_t req = write_responses.front();
    /* build response buffer */
    std::vector<boost::asio::const_buffer> buffer;
    buffer.push_back(boost::asio::buffer(&req.requestId, sizeof(uint32_t)));
    buffer.push_back(boost::asio::buffer(&req.cookieSize, sizeof(uint32_t)));
    buffer.push_back(boost::asio::buffer(req.cookieData, req.cookieSize));

    // send the buffer to the client
    boost::asio::async_write(socket_, buffer,
        [this, req](boost::system::error_code ec, std::size_t length){
            if (!ec) {
                std::cout << "send len: " << length << std::endl;                        
                requests_.erase(req.requestId);
                write_responses.pop();
                if (!write_responses.empty()) {
                    write_message();
                }
            } else {
                std::cerr << "error responding client: " << ec.message() << std::endl;
            }
        }
    );
}

void Session::set_timer(const request_t &req){ 
    auto self(shared_from_this());

    using namespace std::chrono;
    /* sys_time in seconds since the epoch */
    uint64_t sys_time= duration_cast<seconds>(system_clock::now().time_since_epoch()).count();

    std::cout << "Setting timer in " << req.dueTime - sys_time << " " << "seconds\n";

    /* use shared pointer for the timer */
    std::shared_ptr<boost::asio::deadline_timer> timer = std::make_shared<boost::asio::deadline_timer>(context_, boost::posix_time::seconds(req.dueTime - sys_time));
    timers_.insert({req.requestId, timer});

    timer->async_wait(
        [this, self, req](boost::system::error_code ec){
            if (!ec) {
                timers_.erase(req.requestId);
                /* push message in to response queue */
                write_responses.push(req);
                write_message();
            } else {
                std::cerr << "error setting timer: " << ec.message() << " " << ec.value() << std::endl;
            }
        }
    );
}