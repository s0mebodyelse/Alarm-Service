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
            :socket_(std::make_shared<tcp::socket>(io_context))
        {
            try {
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
    req.id = requests_.size() + 1;
    req.timestamp = timestamp;
    req.cookie_data = "Wake me up, before i go go";
    req.cookie_size= req.cookie_data.length();
    requests_.push_back(req);

    uint32_t req_id = htonl(req.id);
    uint32_t req_cookie_size = htonl(req.cookie_size);
    uint32_t high = htonl((uint32_t)(req.timestamp >> 32));
    uint32_t low = htonl((uint32_t)req.timestamp);

    /* build buffer */
    std::vector<boost::asio::const_buffer> message;
    message.push_back(boost::asio::buffer(&req_id, sizeof(uint32_t))); 
    message.push_back(boost::asio::buffer(&high, sizeof(uint32_t))); 
    message.push_back(boost::asio::buffer(&low, sizeof(uint32_t))); 
    message.push_back(boost::asio::buffer(&req_cookie_size, sizeof(uint32_t))); 
    message.push_back(boost::asio::buffer(req.cookie_data));

    uint64_t temp_timestamp = (((uint64_t) ntohl(low)) | ((uint64_t) ntohl(high)) << 32);
    std::cout << "Message: " << req.cookie_data << std::endl;
    std::cout << "Timestamp: " << temp_timestamp << std::endl;

    /* async send the header */
    boost::asio::async_write(*socket_, message,
        [this, &req](boost::system::error_code ec, std::size_t length){
            if (!ec) {
                /* handle the response */
                std::cout << "send len: " << length << std::endl;
                get_response();
            } else {
                std::cerr << "Error occurred sending header: " << ec.message() << std::endl;
            }
        }
    );
}

void Client::get_response() {
    /* read the header first */
    boost::asio::async_read(*socket_, boost::asio::buffer(&inbound_header_.front(), 8), 
        [this] (boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                response_t resp;
                resp.id = this->inbound_header_[0];
                resp.cookie_size = this->inbound_header_[1];
                this->inbound_message_.resize(resp.cookie_size); 
                this->responses_.push_back(resp);

                std::cout << "Server responded: " << resp.id << " " << resp.cookie_size << " " << resp.message << std::endl;

                print_message(resp);
            } else {
                std::cerr << "Error occurred reading response: " << ec.message() << std::endl;
            }
        }
    );
}

void Client::print_message(response_t &resp) {
    boost::asio::async_read(*socket_, boost::asio::buffer(inbound_message_), 
        [this, resp] (boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                std::string cookieData(&inbound_message_[0], inbound_message_.size());
                std::cout << "Timer up: " << cookieData << std::endl;
            } else {
                std::cerr << "Error occurred reading response: " << ec.message() << std::endl;
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
    
        std::cout << "Connecting to Server: " << vm["server"].as<std::string>() << ":" << vm["port"].as<unsigned short>() << std::endl;

        uint64_t timestamp = chron::duration_cast<chron::seconds>(chron::system_clock::now().time_since_epoch()).count() + SECONDS;

        /* start the io context and set a timer */
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
