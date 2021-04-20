#define _WIN32_WINNT 0xA000006

#include <iostream>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>
#include <boost/config.hpp>
#include "server_certificate.hpp"

//using namespace std;
namespace io = boost::asio;
namespace beast = boost::beast;
namespace ssl = io::ssl;
namespace http = beast::http;
using tcp = io::ip::tcp;
using error_code = boost::system::error_code;


class session : std::enable_shared_from_this<session>
{

};

class server : std::enable_shared_from_this<server>
{
public:
	server(io::io_context& io_context)
		: io_context_(io_context),
		acceptor_(io_context)
	{

	}
	void start()
	{
		load_server_certificate(ctx_);

	}
private:
	io::io_context& io_context_;
	ssl::context ctx_{ ssl::context::tlsv13 };
	tcp::acceptor acceptor_;
};

int main()
{

	io::io_context io_context;

	server server(io_context);
	server.start();
	io_context.run();
	return 0;
}