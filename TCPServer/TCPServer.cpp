// #define _WIN32_WINNT 0xA000006
// #define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
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
#include "json.hpp"
//#include "inja.hpp"

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
	session(tcp::socket&& socket) :
		stream_(std::move(socket))
	{
	}
	
	void serve_client()
	{
		io::dispatch(stream_.get_executor(), 
			[self = shared_from_this()]() 
			{
				self->read_client();
			});
		/*read_client();*/
	}
	void close() { 
		beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));
		// stream_.shutdown([self=shared_from_this()](error_code ec) 
		// 	{
		// 		if (ec == boost::asio::ssl::error::stream_truncated)
		// 		{
		// 			return;
		// 		}
		// 		std::cout << ec.message() << std::endl;
		// 	});
		stream_.close();
		return ;
	};
private:

	// void perform_ssl_handshake()
	// {
	// 	beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));
	// 	stream_.async_handshake(ssl::stream_base::server, 
	// 			[self=shared_from_this()](error_code ec) 
	// 			{
	// 			self->ec_ = ec;
	// 			if (ec) {
	// 				//std::cout << "error handshake\n";
	// 			}
	// 			else {
	// 				// self->read_client();
	// 			}
	// 		});

	// }

	void read_client()
	{
	try{
			std::cout << "Reading Client\n";
			request_ = {};
			http::async_read(stream_, flatbuf_, request_, [self = shared_from_this()](error_code ec, std::size_t bytes_transferred) {
			self->ec_ = ec;
			//std::cout << self->request_<<" "<<bytes_transferred;
			if (ec == http::error::end_of_stream)
			{
				self->close();
			}

			self->handle_read();
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

		switch (request_.method())
		{
		case http::verb::get:
		{
			std::cout << "GET request!\n";
			handle_get();

			break;
		};
		case http::verb::post:
		{
			std::cout << "POST request!\n";		
			std::cout << request_.body()<<"\n";
			handle_post(request_.body());
			break;
		};
		case http::verb::put:
		{
			std::cout << "PUT request!\n";
			handle_put();
			break;
		};
		default:
			break;
		}
	}


	bool check_request()
	{
		// check target string
		if (request_.target().empty() ||
			request_.target().front() != '/' ||
			request_.target().find("..") != beast::string_view::npos)
		{
			error_msg = {};
			error_msg.append("target error");
			return false;
		}
		else {
			return true;
		}

	}

	http::response<http::string_body> handle_bad_request()
	{
		http::response<http::string_body> response{ http::status::bad_request,
			request_.version() };
		response.set(http::field::server, "Myserver");
		response.set(http::field::content_type, "text/html");
		response.keep_alive(request_.keep_alive());
		response.body() = error_msg;
		response.prepare_payload();
		return std::move(response);
	}
	http::response<http::string_body> handle_not_found()
	{
		http::response<http::string_body>response{ http::status::not_found,
			request_.version() };
		response.set(http::field::server, "Myserver");
		response.set(http::field::content_type, "text/html");
		response.keep_alive(request_.keep_alive());
		response.body() = error_msg;
		response.prepare_payload();
		return std::move(response);
	}
	http::response<http::string_body> handle_unknown_error() {
		http::response < http::string_body> response{
			http::status::internal_server_error,
			request_.version() };
		response.set(http::field::server, "Myserver");
		response.set(http::field::content_type, "text/html");
		response.keep_alive(request_.keep_alive());
		response.body() = error_msg;
		response.prepare_payload();
		return std::move(response);
	}

	std::string create_item_path(beast::string_view item)
	{
		std::string path{};
		if (item == "/" || item=="/home") {
			path = "index.html";
		}
		else {

			// path.append(item.data(), item.size());
			// for (auto& c : path)
			// 	if (c == '/')
			// 		c = '\\';
			if (path.back() == '\\')
			{
				path = "index.html";
				return path;
			}

		}
		return path;
	}


	void handle_get()
	{

		if (!check_request())
		{
			handle_write(std::move(handle_bad_request()));
			return;
		}

		/*beast::error_code ec;*/
		http::file_body::value_type body;
		/*auto path_res {std::move(create_item_path(request_.target()))};
		*/
		std::cout << "path_res: " << create_item_path(request_.target()).c_str() << "\n";
		body.open(create_item_path(request_.target()).c_str(), beast::file_mode::scan, ec_); // fix file uri

		if (ec_ == beast::errc::no_such_file_or_directory) {
			error_msg = {};
			error_msg = "Nothing is here yet! \n come back later ";
			auto ret_ = handle_not_found();
			handle_write(std::move(ret_));
			return;
		};

		if (ec_)
		{
			error_msg = {};
			error_msg = ec_.message();
			auto ret_ = handle_unknown_error();
			handle_write(std::move(ret_));
			return;
		}
	
		http::response<http::file_body>response{
			std::piecewise_construct,
			std::make_tuple(std::move(body)),
			std::make_tuple(http::status::ok,request_.version())
		};
		response.set(http::field::server, "Myserver");
		response.set(http::field::content_type, "text/html");
		response.content_length(body.size());
		response.keep_alive(request_.keep_alive());
		handle_write(std::move(response));
		return;
	}

	void handle_post(std::string str)
	{
		/*std::ostringstream oss(str);
		oss <<"hello";
		
		std::cout << oss.str();*/
		
		// write to filesystem
		std::string filename = "code.cpp";
		
		std::ofstream os(filename, std::ios::out);
		os << str;
		os.flush();
		os.close();

		std::FILE* fp;
		std::string result{};

		// system("pwd && ls");
		// system("./code");
		fp = popen("g++ -std=c++17 code.cpp -o code && ./code","r");
		
		if(fp==NULL)
		{
			std::cout<<"fp NULLED\n";
		}else{

			char tempbuf_[128];
			while (fgets(tempbuf_, 128, fp)) {
				result += tempbuf_;
			}
		}
		//std::cout << result << std::endl;
		
		nlohmann::json data;
		data["output"] = result.c_str();

		std::cout << data.dump()<<"\n";

		http::response<http::string_body>response{ http::status::ok,
			request_.version() };
		response.set(http::field::server, "Myserver");
		response.set(http::field::content_type, "application/json");
		response.keep_alive(request_.keep_alive());
		response.body() = data.dump();/*"{ \"output\" : \""+result+"\"}";*/
		response.prepare_payload();
		handle_write(std::move(response));
		return;
	}

	void handle_put()
	{

	}

	void handle_write(http::response<http::file_body>&& res_)
	{
		http::write(stream_, res_, ec_);
	}
	void handle_write(http::response<http::string_body>&& res_)
	{
		http::write(stream_,std::move(res_), ec_);
	}

private:
	beast::flat_buffer flatbuf_;
	boost::beast::error_code ec_;
	// beast::ssl_stream<beast::tcp_stream>stream_;
	beast::tcp_stream stream_;
	http::request<http::string_body> request_;
	std::string error_msg;
};

class server :public  std::enable_shared_from_this<server>
{
public:
	server(io::io_context& io_context,short unsigned port)
		: io_context_(io_context),
		acceptor_(io_context, tcp::endpoint{ io::ip::make_address("0.0.0.0"),port })
	{
		std::cout<<"server constructor\n";
		error_code ec;
		acceptor_.listen(io::socket_base::max_listen_connections, ec);
		check("error listenning\n")

	}
	void start()
	{
		// load_server_certificate(ctx_);
		start_accept();
	}
	
	void start_accept()
	{
		
		acceptor_.async_accept(
			[self=shared_from_this()](error_code ec,tcp::socket&& socket) {
			check("error accepting connection\n")
			try
			{
				std::make_shared<session>(std::move((socket)))->serve_client(); 
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
	// ssl::context ctx_{ ssl::context::tlsv13 };
	tcp::acceptor acceptor_;
	tcp::endpoint endpoint_{ io::ip::make_address("127.0.0.1"),8080 };
};

int main(int argc, char* argv[])
{
	std::cout<<"Welcome \n";
	try
	{
		char* port_p=std::getenv("PORT");
		short unsigned port;
		if(port_p){
				port=static_cast<unsigned short>(std::atoi(port_p));
		}else
		{
			port=8080;
		}

		io::io_context io_context;
		std::make_shared<server>(io_context,port)->start();
		io_context.run();
		return 0;
	}
	catch (const error_code& e)
	{
		std::cout<<"what : " << e.message();
	}
	
}
