#define _WIN32_WINNT 0xA000006

#include <iostream>
#include <optional>
#include <boost/asio.hpp>
#include <boost/config.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/filesystem.hpp>
#include "server_certificate.hpp"

//using namespace std;
namespace io = boost::asio;
namespace beast = boost::beast;
namespace ssl = io::ssl;
namespace http = beast::http;
using tcp = io::ip::tcp;
using error_code = boost::system::error_code;

//////////////////////////////

#define check(msg) if(ec){std::cout<<msg;return;}

//////////////////////////////

class session : public std::enable_shared_from_this<session>
{
public:
	session(io::ssl::context& ctx,tcp::socket&& socket) :
		stream_(std::move(socket),ctx)
	{
	}
	
	void serve_client()
	{
		io::dispatch(stream_.get_executor(), 
			[self = shared_from_this()]() 
			{
				self->perform_ssl_handshake();
			});
		/*read_client();*/
	}

private:

	void perform_ssl_handshake()
	{
		beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));
		stream_.async_handshake(ssl::stream_base::server, 
			[self=shared_from_this()](error_code ec) 
			{
			if (ec)
				std::cout << "error handshake\n";
					/*self->stream_.shutdown();*/
				self->read_client();
			});

	}

	void read_client()
	{
		std::cout << "Reading Client\n";
		http::async_read(stream_,streambuf_,request_, [self=shared_from_this()](error_code ec,std::size_t bytes_transferred) {
				std::cout <<&self->streambuf_;
				self->streambuf_.consume(bytes_transferred);
				self->write_client();
				//self->read_client();
			});
		//read_client();
	}

	//io::buffer("HTTP/1.1 200 OK\r\nServer: Beast\r\nContent - Length: 38 \r\n\r\n <html><body>hello world</body></html>")

	void write_client()
	{
		std::cout << "\n\nwriting\n\n";
		http::string_body::value_type body;
		body.append("<html><body>hello world</body></html>");
		auto const size = body.size();
		http::response<http::string_body> res_{
		std::piecewise_construct,
		std::make_tuple(std::move(body)),
		std::make_tuple(http::status::ok, request_.version()) };
		res_.set(http::field::server, BOOST_BEAST_VERSION_STRING);
		res_.set(http::field::content_type, "text / html");
		res_.content_length(size);
		res_.keep_alive(request_.keep_alive());
		http::async_write(stream_, res_, [self = shared_from_this()](error_code ec,std::size_t bytes_transferred)
		{
			self->read_client();

		});

	}
private:
	beast::flat_buffer streambuf_;
	beast::ssl_stream<beast::tcp_stream>stream_;
	http::request<http::string_body> request_;
};

class server :public  std::enable_shared_from_this<server>
{
public:

	

	server(io::io_context& io_context)
		: io_context_(io_context),
		acceptor_(io_context)
	{
		error_code ec;
		acceptor_.open(endpoint_.protocol(),ec);
		check("error openning\n");

		acceptor_.set_option(io::socket_base::reuse_address(true), ec);
		check( "error set_option\n")

		acceptor_.bind(endpoint_, ec);
		check( "error binding\n")

		acceptor_.listen(io::socket_base::max_listen_connections, ec);
		check("error listenning\n")
	}
	void start()
	{

		load_server_certificate(ctx_);
		start_accept();
	}
	
	void start_accept()
	{
		// The new connection gets its own strand
		acceptor_.async_accept(
			io::make_strand(io_context_), [self=shared_from_this()](error_code ec, tcp::socket socket) {
			check("error accepting connection\n")
				std::cout << "\nnew connection\n";
			/*self->v.emplace_back([&]() {self->io_context_.run(); });*/
			
			auto client = std::make_shared<session>(self->ctx_,std::move(socket));
			client->serve_client();
			
			self->start_accept();	
		});
	}

private:
	io::io_context& io_context_;
	ssl::context ctx_{ ssl::context::tlsv13 };
	tcp::acceptor acceptor_;
	tcp::endpoint endpoint_{ io::ip::make_address("127.0.0.1"),8080 };
	
};

int main(int argc, char* argv[])
{
	io::io_context io_context;
	std::make_shared<server>(io_context)->start();
	io_context.run();
	return 0;
}