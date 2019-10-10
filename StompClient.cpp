#include "StompClient.h"
#include "WebSocketSession.h"
#include <iostream>
#include <sstream>
using std::string;


// This is the error callback for right now
void fail( beast::error_code ec, char const* module )
{
  std::cerr << module << ": " << ec.message() << std::endl;
}

void StompClient::onRead( char const *message )
{
  //std::cout << "Received message:\n" << message << std::endl;

  std::string messageText = message;
  std::istringstream messageStream( messageText );
  std::string messageType;
  std::getline( messageStream, messageType );

  std::string header;
  std::map<string,string> headers;
  while( std::getline( messageStream, header ) )
  {
    if( header.length() == 0 )
    {
      break;
    }

    // Parse the header into an attribute-value pair
    size_t colonIndex = header.find( ":" );
    std::string attribute = header.substr( 0, colonIndex );
    std::string value     = header.substr( colonIndex + 1 );
    headers[ attribute ] = value;
  }

  // Get the body, if any
  std::string body;
  std::getline( messageStream, body );

  if( messageType == "CONNECTED" )
  {
    //std::cout << "Connected!" << std::endl;
  }
  else if( messageType == "MESSAGE" )
  {
    std::cout << "Received Message: " << body << std::endl;
    
    // Release the thread lock
    std::unique_lock<std::mutex> locker( g_message );
    g_messagecheck.notify_one();
  }
  else if( messageType == "ERROR" )
  {
    std::cout << "Error! " << body << std::endl;
  }
  else if( messageType == "RECEIPT" )
  {
    //std::cout << "Received receipt for " << headers[ "receipt-id" ] << std::endl;
							      
    // Release the thread lock
    std::unique_lock<std::mutex> locker( g_receipt );
    g_receiptcheck.notify_one();
  }
  
}

const char* StompClient::EOL = "\r\n";

void StompClient::connect( const char* host, const char *port, const char* path, const char *login, const char *passcode )
{
  // Create the WebSocket session
  ioc = new net::io_context();
  
  currentSession = std::make_shared<session>( *ioc, fail, this );

  // Set the various parameters
  currentSession->run( host, port, path );

  // Now start the session running in its own thread
  auto iocRunner = []( net::io_context *ioc )
  {
    ioc->run();
  };

  iocRunnerThread = new std::thread( iocRunner, ioc );

  // Now we need to wait until the connection is ready
  std::unique_lock<std::mutex> locker( currentSession->g_connection );
  currentSession->g_connectioncheck.wait( locker );

  // Create the connection frame
  string connectFrame = makeConnectFrame( "1.1", host, login, passcode );

  // Send the connection frame
  currentSession->send( connectFrame.c_str() );
}


void StompClient::subscribe( int id, char const *destination, char const* ack )
{
  //std::cout << "Subscribing to id " << id << std::endl;
  std::string subscribeFrame = makeSubscribeFrame( id, destination, ack );

  // Send the connection frame
  currentSession->send( subscribeFrame.c_str() );
}


void StompClient::unsubscribe( int id )
{
  //std::cout << "Unsubscribing from id " << id << std::endl;
  std::string unsubscribeFrame = makeUnsubscribeFrame( id );

  // Send the unsubscribe frame
  currentSession->send( unsubscribeFrame.c_str() );
}

// Send a message to the server
void StompClient::send( char const* destination, char const *contentType, char const *body )
{
  std::cout << "Sending message " << body << std::endl;
  std::string sendFrame = makeSendFrame( destination, contentType, body );

  // Send the message
  currentSession->send( sendFrame.c_str() );
}

// Disconnect from the WebSocket
void StompClient::disconnect( int receipt )
{
  string disconnectFrame = makeDisconnectFrame( receipt );
  currentSession->send( disconnectFrame.c_str() );
}

// Close the WebSocket
void StompClient::close()
{
  //std::cout << "Closing WebSocket" << std::endl;
  currentSession->close();
}

// This is used by the client to "wait" for all the operations to finish
void StompClient::synchronize()
{
  //std::cout << "Waiting on ioc thread" << std::endl;
  iocRunnerThread->join();
}


// Wait for a message to be received
void StompClient::synchronizeMessage()
{
  std::unique_lock<std::mutex> locker( g_message );
  g_messagecheck.wait( locker );
}

// Wait for a receipt to be received
void StompClient::synchronizeReceipt()
{
  std::unique_lock<std::mutex> locker( g_receipt );
  g_receiptcheck.wait( locker );
}
  
// Helper functions
string StompClient::makeConnectFrame( const char* version, const char* host, const char *login, const char *passcode )
{
  string frame = "CONNECT";
  frame += EOL;
  frame += "accept-version:";
  frame += version;
  frame += EOL;
  frame += "host:";
  frame += host;
  frame += EOL;
  frame += "heart-beat:0,0";
  frame += EOL;
  frame += "login:";
  if( login != NULL )
  {
    frame += login;
  }
  else
  {
    frame += "None";
  }
  frame += EOL;

  frame += "passcode:";
  if( passcode != NULL )
  {
    frame += passcode;
  }
  else
  {
    frame += "None";
  }
  frame += EOL;
  frame += EOL;

  return frame;  
}

string StompClient::makeSubscribeFrame( int id, const char *destination, const char* ack )
{
  string frame = "SUBSCRIBE";
  frame += EOL;
  frame += "id:";
  frame += std::to_string( id );
  frame += EOL;
  frame += "destination:";
  frame += destination;
  frame += EOL;
  if( ack != NULL )
  {
    frame += "ack:";
    frame += ack;
  }
  else
  {
    frame += "ack:auto";
  }
  frame += EOL;
  frame += EOL;
  return frame;
}


string StompClient::makeSendFrame( const char* destination, const char* contentType, const char *body )
{
  string frame = "SEND";
  frame += EOL;
  frame += "destination:";
  frame += destination;
  frame += EOL;
  frame += "content-type:";
  frame += contentType;
  frame += EOL;
  int frameLength = 0;
  if( body != NULL )
  {
    frameLength += strlen( body ) + 1;
  }

  // Need to add yet one more 1 to account for the '\n'
  // terminating the message body.
  frame += "content-length:";
  frame += std::to_string( frameLength + 1 );
  frame += EOL;
  frame += EOL;
  if( body != NULL )
  {
    frame += body;
  }

  frame += EOL;
  return frame;
}


string StompClient::makeUnsubscribeFrame( int id )
{
  string frame = "UNSUBSCRIBE";
  frame += EOL;
  frame += "id:";
  frame += std::to_string( id );
  frame += EOL;
  frame += EOL;
  return frame;
}
  

string StompClient::makeDisconnectFrame( int receipt )
{
  string frame = "DISCONNECT";
  frame += EOL;
  frame += "receipt:";
  frame += std::to_string( receipt );
  frame += EOL;
  frame += EOL;
  return frame;
}

