#pragma once

// Standard includes
#include <string>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory> 
#include <thread>
#include <condition_variable>
#include <mutex>
#include <deque>

// Imports from boost/beast
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/strand.hpp>

// WebSockets
#include "WebSocketCallbacks.h"

// Declare the namespaces
namespace beast     = boost::beast;
namespace http      = beast::http;
namespace websocket = beast::websocket;
namespace net       = boost::asio;
using     tcp       = boost::asio::ip::tcp;

class session : public std::enable_shared_from_this<session>
{
 public:
  // Constructor
  explicit session( net::io_context &ioc, void (*errorFunction)( beast::error_code ec, char const *module ),
		    websocketcallbacks *callbacks );

  // Destructor
  ~session();
  
  void run( char const* host, char const*port, char const* path );
  void on_resolve( beast::error_code ec, tcp::resolver::results_type results );
  void on_connect( beast::error_code ec, tcp::resolver::results_type::endpoint_type results );
  void on_handshake( beast::error_code ec );
  void on_write( beast::error_code ec, std::size_t bytes_transferred );
  void on_read(  beast::error_code ec, std::size_t bytes_transferred );
  void on_close( beast::error_code ec );
  void queueRead();

  // External APIs
  void send( char const* newText );
  void close();

  // These are used to cause the client to wait for the connection to be made.
  std::mutex              g_connection;
  std::condition_variable g_connectioncheck;
  std::mutex              g_messages;
  std::mutex              g_write;
  std::condition_variable g_writecheck;

 private:
  tcp::resolver                        resolver_;
  websocket::stream<beast::tcp_stream> ws_;
  beast::flat_buffer                   buffer_;
  std::string                          host_;
  std::string                          path_;
  void (*errorFunction_)( beast::error_code ec, char const *module );
  websocketcallbacks                  *callbacks_;
  std::deque< std::string>             messagesToSend;
};
  






  
