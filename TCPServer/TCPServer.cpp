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
#include <boost/exception/all.hpp>
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

#define check(msg) if(ec){std::cout<<msg;}

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
	~session() { 
		beast::close_socket(beast::get_lowest_layer(stream_));
		//stream_.async_shutdown([](error_code ec) 
		//	{
		//		if (ec != boost::asio::ssl::error::stream_truncated)
		//		{
		//			std::cout << ec.message() << std::endl;
		//			/*throw boost::system::system_error{ ec };*/
		//		}
		//	});
	};
private:

	void perform_ssl_handshake()
	{
		beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));
		stream_.async_handshake(ssl::stream_base::server, 
				[self=shared_from_this()](error_code ec) 
				{
				self->ec = ec;
				if (ec) {
					//std::cout << "error handshake\n";
				}
				else {
					self->read_client();
				}
			});

	}

	void read_client()
	{
		try{
;			std::cout << "Reading Client\n";
			request_ = {};
			http::async_read(stream_,flatbuf_,request_, [self=shared_from_this()](error_code ec,std::size_t bytes_transferred) {
					self->ec = ec;
					//std::cout << self->request_<<" "<<bytes_transferred;
					self->handle_read();
					self->write_client();
				});
		}
		catch (const boost::system::system_error& ec)
		{
			//std::cout << "\n\nstart_accept: " << ec.what();
			if (ec.code() == boost::asio::error::connection_aborted)
			{
				
			}
		}
	}

	void handle_read()
	{
		std::cout<<request_.target();
	}

	void handle_write()
	{

	}

	void write_client()
	{
		try {
			std::cout << "\nwriting\n";
			http::string_body::value_type body;
			body.append("<html><body>hello world<a href=""https://www.w3schools.com"">Visit W3Schools</a>\
								<input type=""text"" id=""name:"" name=""name"" required\
								minlength = ""4"" maxlength = ""8"" size = ""10"" > </body></html>");
			auto const size = body.size();
			http::response<http::string_body> res_{
			std::piecewise_construct,
			std::make_tuple(std::move(body)),
			std::make_tuple(http::status::ok, request_.version()) };
			res_.set(http::field::server, BOOST_BEAST_VERSION_STRING);
			res_.set(http::field::content_type, "text / html");
			res_.content_length(size);
			res_.keep_alive(request_.keep_alive());
			
			http::write(stream_, res_, ec);
			/*http::async_write(stream_, res_, [self=shared_from_this()](error_code ec,std::size_t bytes_transferred) {
					
					self -> ec = ec;
					self->read_client();
				});*/
		}
		catch (const boost::system::system_error& ec)
		{
			std::cout << "\nstart_accept: " << ec.what();
		}
		catch (const std::exception& e)
		{
			
			std::cout << "\nsfskladf " << e.what();
		}
	}
	
private:
	beast::flat_buffer flatbuf_;
	boost::beast::error_code ec;
	beast::ssl_stream<beast::tcp_stream>stream_;
	http::request<http::string_body> request_;
};

class server :public  std::enable_shared_from_this<server>
{
public:
	server(io::io_context& io_context)
		: io_context_(io_context),
		acceptor_(io_context, tcp::endpoint{ io::ip::make_address("127.0.0.1"),8080 })
	{
		error_code ec;
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
		/*socket.emplace(io::make_strand(io_context_));*/
		//std::shared_ptr<tcp::socket> socket(new tcp::socket(io::make_strand(io_context_)));
		acceptor_.async_accept(
			[self=shared_from_this()](error_code ec,tcp::socket&& socket) {
			check("error accepting connection\n")
			//std::cout << "\nnew connection\n";
			try
			{
				std::make_shared<session>(self->ctx_, std::move((socket)))->serve_client(); 
				self->start_accept();
			}
			catch (const error_code &ec)
			{
				std::cout << "\n\nstart_accept: " << ec.message();
			}		
		});
	}

private:
	io::io_context& io_context_;
	ssl::context ctx_{ ssl::context::tlsv13 };
	tcp::acceptor acceptor_;
	tcp::endpoint endpoint_{ io::ip::make_address("127.0.0.1"),8080 };
	//std::optional<tcp::socket> socket;
};

int main(int argc, char* argv[])
{
	try
	{
		io::io_context io_context;
		std::make_shared<server>(io_context)->start();
		io_context.run();
		return 0;
	}
	catch (const error_code& e)
	{
		std::cout<<"what : " << e.message();
	}
	
}