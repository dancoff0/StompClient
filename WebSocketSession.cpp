#include "WebSocketSession.h"
#include <cstring>
 
// Constructor
session::session( net::io_context& ioc, void (*errorFunction)(beast::error_code, char const*) ,
		  websocketcallbacks *callbacks )
  : resolver_( net::make_strand( ioc ) ), ws_( net::make_strand( ioc ) ),
    errorFunction_( errorFunction ), callbacks_( callbacks )
{
}

// Destructor
session::~session()
{
  //std::cout << "Session destructor called" << std::endl;
}

// Start the asynchronous operation
void session::run( char const *host, char const *port, char const *path )
{
  // Save the arguments
  host_ = host;
  path_ = path;

  //std::cout << "Starting to resolve " << host << " at port " << port << " with path " << path << std::endl;
    
  // Look up the domain
  resolver_.async_resolve( host, port, beast::bind_front_handler( &session::on_resolve, shared_from_this() ) );
  
  //std::cout << "Finished sending asynchronous resolve request" << std::endl;
}

// Next step, after the host has been resolved is to connect to the server
void session::on_resolve( beast::error_code ec, tcp::resolver::results_type results )
{
  //std::cout << "Resolved server" << std::endl;
  
  if( ec )
  {
    return (*errorFunction_)( ec, "resolve" );
  }

  // Set the timeout to be 30 seconds.
  beast::get_lowest_layer( ws_ ).expires_after( std::chrono::seconds( 30 ) );

  // Make the IP connection
  //std::cout << "Connecting to server " << std::endl;
  
  beast::get_lowest_layer( ws_ ).async_connect( results, beast::bind_front_handler( &session::on_connect, shared_from_this()));
}

// Prepared the connection
void session::on_connect( beast::error_code ec, tcp::resolver::results_type::endpoint_type results )
{
  //std::cout << "Connected" << std::endl;
  
  if( ec )
  {
    return (*errorFunction_)( ec, "connect" );
  }

  // Turn off the timeout on the tcp_stream
  beast::get_lowest_layer( ws_ ).expires_never();

  // Set the suggested timeout on the websocket
  ws_.set_option( websocket::stream_base::timeout::suggested( beast::role_type::client ) );

  // Change the name of the user agent
  ws_.set_option( websocket::stream_base::decorator( [](websocket::request_type& req)
		{
		  req.set( http::field::user_agent, std::string( BOOST_BEAST_VERSION_STRING ) + " websocket-client-async" );
		}));

  // Perform the websocket "upgrade" dance and handshake.
  ws_.async_handshake( host_, path_, beast::bind_front_handler( &session::on_handshake, shared_from_this() ) );
}

void session::on_handshake( beast::error_code ec )
{
  if( ec )
  {
    return (*errorFunction_)( ec, "handshake" );
  }

  // We are a web socket!
  std::unique_lock<std::mutex> locker( g_connection );
  g_connectioncheck.notify_one();

  //std::cout << "Connection is ready: notifying the client" << std::endl;
  
  // Queue up an asynchronous read.
  ws_.async_read( buffer_, beast::bind_front_handler( &session::on_read, shared_from_this() ) );
}


// Clean up after a write operation
void session::on_write( beast::error_code ec, std::size_t bytes_transferred )
{
  //std::cout << "Sucessfully completed write operation" << std::endl;
  // Clean up the buffer
  boost::ignore_unused( bytes_transferred );

  if( ec )
  {
    return (*errorFunction_)( ec, "write" );
  }

  // Release the semaphore lock
  std::unique_lock<std::mutex> locker( g_write );
  g_writecheck.notify_one();
}

typedef void (session::*queueReadFunction)();

// Clean up after a read operation
void session::on_read( beast::error_code ec, std::size_t bytes_transferred )
{
  //std::cout << "OnRead called" << std::endl;
  
  boost::ignore_unused( bytes_transferred );

  if( ec )
  {
    return (*errorFunction_)( ec, "read" );
  }

  // Get the data
  std::string serverString = beast::buffers_to_string( buffer_.data() );
  const char* serverText = serverString.c_str();

  // Clear out the buffer
  buffer_.clear();

  // Queue up another read and ...
  ws_.async_read( buffer_, beast::bind_front_handler( &session::on_read, shared_from_this() ));

  // ... handle the message
  callbacks_->onRead( serverText );
}

/*
void session::queueRead()
{
  ws_.async_read( buffer_, beast::bind_front_handler( &session::on_read, shared_from_this() ));
}
*/

void session::on_close( beast::error_code ec )
{
  if( ec )
  {
    return (*errorFunction_)( ec, "close" );
  }
}

// External APIs
void session::send( char const* newText )
{
  std::cout << "Writing new text:\n" << std::string( newText ) << std::endl;

  // Add the '\0' to end the frame.  
  std::string textToSend( newText );
  textToSend.push_back( '\0' );
  textToSend.push_back( '\n' );

  // Do the write and block until the write is finished.
  ws_.async_write( net::buffer( textToSend ), beast::bind_front_handler( &session::on_write, shared_from_this() ) );

  // This causes the calling thread to block until the write is done.
  std::unique_lock<std::mutex> locker( g_write );
  g_writecheck.wait( locker );
}

void session::close()
{
  std::cout << "WebSocketSession: closing WebSocket" << std:: endl;
  ws_.async_close( websocket::close_code::normal, beast::bind_front_handler( &session::on_close, shared_from_this() ) );
}
    
      
						    
