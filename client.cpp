#include <iostream>
#include <chrono>
#include <thread>
#include <queue>

#include <boost/program_options.hpp>
#include <boost/asio.hpp>

#define SECONDS 10

using boost::asio::ip::tcp;
namespace bpo = boost::program_options;
namespace chron = std::chrono;

bpo::variables_map vm;

typedef struct {
    uint32_t id;
    uint64_t timestamp;
    uint32_t low_time;
    uint32_t high_time;
    uint32_t cookie_size;
    std::string cookie_data;

    std::vector<boost::asio::const_buffer> build_buffer() {
        std::vector<boost::asio::const_buffer> buffer;

        buffer.push_back(boost::asio::buffer(&id, sizeof(uint32_t)));
        buffer.push_back(boost::asio::buffer(&high_time, sizeof(uint32_t)));
        buffer.push_back(boost::asio::buffer(&low_time, sizeof(uint32_t)));
        buffer.push_back(boost::asio::buffer(&cookie_size, sizeof(uint32_t)));
        buffer.push_back(boost::asio::buffer(cookie_data));

        return buffer;
    }
} request_t;

typedef struct {
    uint32_t id;
    uint32_t cookie_size;
    std::string message;
} response_t;

/* connects to server */
class Client {
    public:
        Client(boost::asio::io_context &io_context, const tcp::resolver::results_type &endpoints) 
            :socket_(io_context), io_context_(io_context)
        {
            do_connect(endpoints);
        }

        /* set a timer with a unix timestamp */
        void set_timer(uint64_t timestamp, std::string cookieData);
        void close();

    private:
        tcp::socket socket_;
        boost::asio::io_context &io_context_;

        /* buffers for incoming messages */
        std::vector<uint32_t> inbound_header_{0,0};
        std::string inbound_message_;

        /* acts as queue for requests */
        std::queue<request_t> requests_;

        void do_connect(const tcp::resolver::results_type &endpoints);
        void do_write_request();
        void do_read_header();
        void do_read_body(response_t resp);
};

void Client::close() {
    boost::asio::post(io_context_, [this](){ socket_.close(); });
}

void Client::do_connect(const tcp::resolver::results_type &endpoints) {
    boost::asio::async_connect(socket_, endpoints, 
        [this] (boost::system::error_code ec, tcp::endpoint) {
            if (!ec) {
                do_read_header();
            } else {
                std::cerr << "Error connecting: " << ec.message() << std::endl;
            }
        }
    );
}

void Client::set_timer(uint64_t timestamp, std::string cookieData) {
    /* build the request */
    request_t req;
    req.id = htonl(requests_.size() + 1);
    req.timestamp = timestamp;
    req.high_time = htonl((uint32_t)(req.timestamp >> 32));
    req.low_time = htonl((uint32_t)req.timestamp);
    req.cookie_data = cookieData;
    req.cookie_size= htonl(req.cookie_data.length());

    std::cout << "Setting timer: " << ntohl(req.id) << " " << req.timestamp << " " << req.cookie_data << std::endl;
    
    boost::asio::post(io_context_, 
        [this, req] () {
            /* push request to queue and start writing */ 
            bool write_in_progress = !requests_.empty();
            requests_.push(req);
            if (!write_in_progress) {
                do_write_request();
            } 
        } 
    );
}

void Client::do_write_request() {
    /* async send the request*/
    boost::asio::async_write(socket_, requests_.front().build_buffer(),
        [this](boost::system::error_code ec, std::size_t length){
            if (!ec) {
                /* request send success, remove from queue*/
                requests_.pop();
                if (!requests_.empty()) {
                    do_write_request();
                }

                std::cout << "Request is written" << std::endl;
            } else {
                std::cerr << "Error occurred writing request: " << ec.message() << std::endl;
                socket_.close();
            }
        }
    );
}

void Client::do_read_header() {
    /* read the header first */
    boost::asio::async_read(socket_, boost::asio::buffer(&inbound_header_.front(), 8), 
        [this] (boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                response_t resp;
                resp.id = this->inbound_header_[0];
                resp.cookie_size = this->inbound_header_[1];
                this->inbound_message_.resize(resp.cookie_size); 

                std::cout << "Server responded: " << resp.id << " " << resp.cookie_size << " " << resp.message << std::endl;

                do_read_body(resp);
            } else {
                std::cerr << "Error occurred reading response header: " << ec.message() << std::endl;
                socket_.close();
            }
        }
    );
}

void Client::do_read_body(response_t resp) {
    boost::asio::async_read(socket_, boost::asio::buffer(inbound_message_), 
        [this, resp] (boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                std::string cookieData(&inbound_message_[0], inbound_message_.size());
                std::cout << "Timer up: " << cookieData << std::endl;
                do_read_header();
            } else {
                std::cerr << "Error occurred reading response body: " << ec.message() << std::endl;
                socket_.close();
            }
        }
    );
}

int main(int argc, const char *argv[]) {
    try {
        /* reading cli arguments */
        bpo::options_description description("List of options");
        description.add_options()
            ("help,h", "prints help message")
            ("server,s", bpo::value<std::string>(), "adress of the timer server")        
            ("port,p", bpo::value<std::string>(), "port the server is listeing on")
            ("messages,m", bpo::value<int>()->default_value(1), "number of messages to send async")
        ;

        bpo::store(bpo::parse_command_line(argc, argv, description), vm);
        bpo::notify(vm);

        if (vm.count("help")) {
            std::cout << description << std::endl;
            return 0;
        }

        if (!vm.count("server") || !vm.count("port")) {
            std::cout << "Please provide a server and a port!" << std::endl;
            std::cout << description << std::endl;
            return 1;
        }
    
        std::cout << "Connecting to Server: " << vm["server"].as<std::string>() << ":" << vm["port"].as<std::string>() << std::endl;
        
        uint64_t timestamp = chron::duration_cast<chron::seconds>(chron::system_clock::now().time_since_epoch()).count() + SECONDS;
        std::string first_data = "this is message 1";
        std::string second_data = "this is message 2";

        boost::asio::io_service io_context;

        /* connect to server */
        tcp::resolver res(io_context);
        auto endpoints = res.resolve(vm["server"].as<std::string>(), vm["port"].as<std::string>());

        /* create the client, the io_service is scheduled with the async connect */
        Client timer_client(io_context, endpoints);

        std::thread t([&io_context](){io_context.run();});

        if (vm["messages"].as<int>() == 2) {
            timer_client.set_timer(timestamp, first_data);
            timer_client.set_timer(timestamp + 2, second_data);
        } else {
            timer_client.set_timer(timestamp, first_data);
        }


        t.join();
    } catch (std::exception &e) {
        std::cerr << "Error occurred in main: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
