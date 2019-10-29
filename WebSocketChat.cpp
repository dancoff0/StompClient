#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/strand.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string> 
#include <gtk/gtk.h>
#include <thread>

namespace beast     = boost::beast;         // from <boost/beast.hpp>
namespace http      = beast::http;          // from <boost/beast/http.hpp>
namespace websocket = beast::websocket;     // from <boost/beast/websocket.hpp>
namespace net       = boost::asio;          // from <boost/asio.hpp>
using     tcp       = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

// These are global so that they are easily accessible from the "sendText" callback.
GtkTextIter    iter;
GtkTextBuffer *buffer;

// Report a failure
void fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// Sends a WebSocket message and prints the response
class session : public std::enable_shared_from_this<session>
{
    tcp::resolver resolver_;
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    std::string host_;
    std::string path_;
    
public:
  // Resolver and socket require an io_context
  explicit session(net::io_context& ioc) : resolver_(net::make_strand(ioc)), ws_(net::make_strand(ioc))
  {
  }

  // Start the asynchronous operation
  void run( char const* host, char const* port,  char const* path )
  {
    // Save these for later
    host_ = host;
    path_ = path;

    g_print( "host = %s, port = %s, path = %s\n", host, port, path );
    // Look up the domain name
    resolver_.async_resolve( host, port, beast::bind_front_handler( &session::on_resolve, shared_from_this()) );
  }
  
  // We will come here once the host is resolved.
  void on_resolve( beast::error_code ec, tcp::resolver::results_type results )
  {
    if(ec)
      return fail(ec, "resolve");
    
    // Set the timeout for the operation
    beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));
    
    // Make the connection on the IP address we get from a lookup
    beast::get_lowest_layer(ws_).async_connect( results, beast::bind_front_handler( &session::on_connect, shared_from_this()));
  }
  
  void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type)
  {
    if(ec)
      return fail(ec, "connect");
    
    // Turn off the timeout on the tcp_stream, because
    // the websocket stream has its own timeout system.
    beast::get_lowest_layer(ws_).expires_never();
    
    // Set suggested timeout settings for the websocket
    ws_.set_option( websocket::stream_base::timeout::suggested( beast::role_type::client) );
    
    // Set a decorator to change the User-Agent of the handshake
    ws_.set_option(websocket::stream_base::decorator([](websocket::request_type& req)
		  {
		    req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) +" websocket-client-async");
		  }));

    g_print( "Connecting to path %s\n", path_.c_str() );
    
    // Perform the websocket handshake
    ws_.async_handshake(host_, path_, beast::bind_front_handler( &session::on_handshake, shared_from_this()));
  }
  
  void on_handshake(beast::error_code ec)
  {
    if(ec)
      return fail(ec, "handshake");
    g_print( "Completed WebSocket handshake\n" );
    
   // Set up to read a message into our buffer
    ws_.async_read( buffer_, beast::bind_front_handler( &session::on_read, shared_from_this()));
  }

  // Text has been sent
  void on_write( beast::error_code ec, std::size_t bytes_transferred)
  {
    boost::ignore_unused(bytes_transferred);

    if(ec)
      return fail(ec, "write");
  }
       
  void on_read( beast::error_code ec, std::size_t bytes_transferred)
  {
    boost::ignore_unused(bytes_transferred);

    if(ec)
      return fail(ec, "read");

    // Get the data
    std::string serverString = beast::buffers_to_string( buffer_.data() );
    const char* serverText = serverString.c_str();
    g_print( "Got text from server: %s\n", serverText );

    // Clear the websocket buffer.
    buffer_.clear();

    gtk_text_buffer_insert( buffer, &iter, serverText, -1 );
    gtk_text_buffer_insert( buffer, &iter, "\n",       -1 );

    // Queue up another read.
    ws_.async_read( buffer_, beast::bind_front_handler( &session::on_read, shared_from_this()));
  }

  void on_close(beast::error_code ec)
  {
    if(ec)
      return fail(ec, "close");

    // If we get here then the connection is closed gracefully
    g_print( "WebSocket closed" );
  }

  void send( const char* newText )
  {
    ws_.async_write( net::buffer( std::string(newText) ), beast::bind_front_handler( &session::on_write, shared_from_this()));
  }

  void close()
  {
    ws_.async_close(websocket::close_code::normal, beast::bind_front_handler( &session::on_close, shared_from_this()));
  }

};

std::shared_ptr<session> currentSession;

// Send text to the server
void sendText( GtkEntry *entry, gpointer data )
{
  const char* newText = gtk_entry_get_text( GTK_ENTRY( entry ) );
  g_print( "Text in entry box: %s\n", newText ); 
  
  // Send the message
  currentSession->send( newText );
  gtk_entry_set_text( entry, "" );
}

void destroy( GtkWidget *widget, gpointer data )
{
  // Close the WebSocket connection
  currentSession->close();

  // Now close the GTK application
  gtk_main_quit();
}

/*
void iocRunner( net::io_context *ioc )
{
  ioc->run();
  g_print( "iocRunner: done\n" );
}
*/

int main( int argc, char *argv[] )
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *entry;
  GtkWidget *view;

  // Initialize the application
  gtk_init( &argc, &argv );

  // Create the window and set its size.
  window = gtk_window_new( GTK_WINDOW_TOPLEVEL );
  gtk_window_set_position( GTK_WINDOW( window ), GTK_WIN_POS_CENTER );
  gtk_window_set_default_size( GTK_WINDOW( window ), 500, 500 );
  gtk_window_set_title( GTK_WINDOW( window ), "WDTS WebSockets Chat" );

  // Create the vertical box as a layout manager
  vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );

  // Create the TextView widget and add it to the vbox.
  view = gtk_text_view_new();
  gtk_box_pack_start( GTK_BOX( vbox ), view, TRUE, TRUE, 0 );

  // Get the buffer in the TextView and create an iterator that starts from the beginning.
  buffer = gtk_text_view_get_buffer( GTK_TEXT_VIEW( view ) );
  gtk_text_buffer_get_iter_at_offset( buffer, &iter, 0 );

  
  // Create an Entry for us to type into.
  entry = gtk_entry_new();

  // Add it to the vbox, but do not let it expand.
  gtk_box_pack_start( GTK_BOX( vbox ), entry, FALSE, FALSE, 0 );

  // The vbox is full, so add it to the window.
  gtk_container_add( GTK_CONTAINER( window ), vbox );

  // Connect the call backs
  g_signal_connect( entry, "activate", G_CALLBACK( sendText ), NULL );
  g_signal_connect( window, "destroy", G_CALLBACK( destroy ),  NULL );

  // Show everything
  gtk_widget_show_all( window );

  // Create the WebSocket Session
  net::io_context ioc;

  currentSession = std::make_shared<session>(ioc);

  currentSession->run( "172.16.2.21", "8080", "/ChatServer/ws" );

  // ioc run will be need to be in its own thread.
  
  auto iocRunner = []( net::io_context *ioc )
  {
      ioc->run();
  };
  
  std::thread iocRunnerThread( iocRunner, &ioc );
  
  /*
  auto gtkRunner = []()
    {
      gtk_main();
    };

  std::thread gtkRunnerThread( gtkRunner, NULL );
  */

  // Start the GTK part of the application.
  gtk_main();

  g_print( "Starting ioc\n" );
  //ioc.run();

  return 0;
}

  
  
  
