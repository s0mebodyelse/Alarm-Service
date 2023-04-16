#include <iostream>
#include <chrono>
#include <boost/program_options.hpp>
#include <boost/asio.hpp>

#define SECONDS 5

using boost::asio::ip::tcp;
namespace bpo = boost::program_options;
namespace chron = std::chrono;

bpo::variables_map vm;

typedef struct {
    uint32_t id;
    uint64_t timestamp;
    uint32_t cookie_size;
    std::string cookie_data;
} request_t;

typedef struct {
    uint32_t id;
    uint32_t cookie_size;
    std::string message;
} response_t;

/* connects to server */
class Client {
    public:
        Client(boost::asio::io_context &io_context) 
        {
            try {
                socket_ = std::make_shared<tcp::socket>(io_context);
                socket_->connect(tcp::endpoint(boost::asio::ip::address::from_string(vm["server"].as<std::string>()), vm["port"].as<unsigned short>()));
            } catch (std::exception &e) {
                std::cerr << "Error occurred during Client construction: " << e.what() << std::endl;
                throw;
            }
        }

        /* set a timer with a unix timestamp */
        void set_timer(uint64_t timestamp);

    private:
        std::shared_ptr<tcp::socket> socket_;

        /* buffers for incoming messages */
        std::vector<uint32_t> inbound_header_{0,0};
        std::string inbound_message_;

        /* keep track of the running requests */
        std::vector<request_t> requests_;
        std::vector<response_t> responses_;

        void get_response();
        void print_message(response_t &resp);
};

void Client::set_timer(uint64_t timestamp) {
    request_t req;
    req.id = htonl(requests_.size() + 1);
    req.timestamp = timestamp;
    req.cookie_data = "Wake me up";
    req.cookie_size= htonl(req.cookie_data.length());

    requests_.push_back(req);

    /* split timestamp */
    uint32_t low = htonl((uint32_t)(req.timestamp >> 32));
    uint32_t high = htonl((uint32_t)req.timestamp);

    /* build buffer */
    std::vector<boost::asio::const_buffer> message;
    message.push_back(boost::asio::buffer(&req.id, sizeof(uint32_t))); 
    message.push_back(boost::asio::buffer(&high, sizeof(uint32_t))); 
    message.push_back(boost::asio::buffer(&low, sizeof(uint32_t))); 
    message.push_back(boost::asio::buffer(&req.cookie_size, sizeof(uint32_t))); 
    message.push_back(boost::asio::buffer(req.cookie_data));

    std::cout << "Setting Timer with ID: " << req.id << " and Timestamp: " << timestamp << std::endl;

    /* async send the header */
    socket_->async_send(boost::asio::buffer(message),
        [this, &req](boost::system::error_code ec, std::size_t length){
            if (!ec) {
                /* handle the response */
                get_response();
            } else {
                std::cerr << "Error occurred sending header: " << ec.message() << std::endl;
            }
        }
    );
}

void Client::get_response() {
    /* read the header first */
    socket_->async_receive(boost::asio::buffer(&inbound_header_.front(), 8), 
        [this] (boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                response_t resp;
                resp.id = this->inbound_header_[0];
                resp.cookie_size = this->inbound_header_[1];
                this->inbound_message_.resize(resp.cookie_size); 
                this->responses_.push_back(resp);
                print_message(resp);
            } else {
                std::cerr << "Error occurred reading response: " << ec.message() << std::endl;
            }
        }
    );
}

void Client::print_message(response_t &resp) {
    socket_->async_receive(boost::asio::buffer(inbound_message_), 
        [this, resp] (boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                std::cout << "Timer up: " << resp.message << std::endl;
            } else {
                std::cerr << "Error occurred reading response: " << ec.message() << std::endl;
            }
        }
    );
}

/*
void read_message(const boost::system::error_code &ec, std::size_t bytes_transferred) {
    if (!ec) {
        std::cout << "Response: " << inbound_message << std::endl;
    }
}

void read_header(const boost::system::error_code &ec, std::size_t bytes_transferred) {
    if (!ec) {
        inbound_message.resize(inbound_header[1]);
        boost::asio::async_read(tcp_socket, boost::asio::buffer(inbound_message, inbound_header[1]), read_message);
    }
}

void connect_handler(const boost::system::error_code &ec) {
    if (!ec) {

        uint32_t request_id = 1223;
        uint64_t unix_timestamp = chron::duration_cast<chron::seconds>(chron::system_clock::now().time_since_epoch()).count() + SECONDS;
        std::string cookie_data = "Wake me up";
        uint32_t cookie_size = cookie_data.length();

        uint32_t low = (uint32_t)(unix_timestamp >> 32);
        uint32_t high = (uint32_t)unix_timestamp;

        std::vector<boost::asio::const_buffer> header;
        header.push_back(boost::asio::buffer(&request_id, sizeof(uint32_t))); 
        header.push_back(boost::asio::buffer(&low, sizeof(uint32_t))); 
        header.push_back(boost::asio::buffer(&high, sizeof(uint32_t))); 
        header.push_back(boost::asio::buffer(&cookie_size, sizeof(uint32_t))); 

        boost::asio::write(tcp_socket, header);
        boost::asio::write(tcp_socket, boost::asio::buffer(cookie_data));

        boost::asio::async_read(tcp_socket, boost::asio::buffer(&inbound_header.front(), 8), read_header);

    }
}

void resolve_handler(const boost::system::error_code &ec, tcp::resolver::iterator it) {
    if (!ec) {
        tcp_socket.async_connect(*it, connect_handler);
    } else {
        std::cout << ec.message() << std::endl;
    }
}
*/

int main(int argc, const char *argv[]) {
    
    try {
        /* reading cli arguments */
        bpo::options_description description("List of options");
        description.add_options()
            ("help,h", "prints help message")
            ("server,s", bpo::value<std::string>(), "adress of the timer server")        
            ("port,p", bpo::value<unsigned short>(), "port the server is listeing on")
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
    
        /* start the io context and send the messages */ 
        std::cout << "Connecting to Server: " << vm["server"].as<std::string>() << ":" << vm["port"].as<unsigned short>() << std::endl;
        uint64_t timestamp = chron::duration_cast<chron::seconds>(chron::system_clock::now().time_since_epoch()).count() + SECONDS;

        boost::asio::io_service io_context;
        Client timer_client(io_context);
        timer_client.set_timer(timestamp);
        io_context.run();
    } catch (std::exception &e) {
        std::cerr << "Error occurred in main: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}