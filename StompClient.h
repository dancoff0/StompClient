#pragma once

// Standard includes
#include <string>
using std::string;

// Websocket include
#include "WebSocketSession.h"
#include "WebSocketCallbacks.h"

class StompClient : public websocketcallbacks
{
 public:
  void connect( const char* host, const char *port, const char* path, const char *login, const char *passcode );
  void subscribe( int id, const char *destination, const char* ack );
  void send( const char* destination, const char* contentType, const char *body );
  void unsubscribe( int id );
  void disconnect( int receipt );
  void close();
  void synchronizeMessage();
  void synchronizeReceipt();
  void synchronize();

  // Set the message handlers
  void setMessageHandler( void (*handler)(string body) );
  
  // Callbacks
  void onRead( char const* message );

  // These are used to force synchronous receipt of messages and receipts
  std::condition_variable g_messagecheck;
  std::mutex              g_message;
  std::condition_variable g_receiptcheck;
  std::mutex              g_receipt;

 private:
  // Helper functions
  string makeConnectFrame( const char* version, const char* host, const char *login, const char *passcode );
  string makeSubscribeFrame( int id, const char *destination, const char* ack );
  string makeSendFrame( const char* destination, const char* contentType, const char *body );
  string makeUnsubscribeFrame( int id );
  string makeDisconnectFrame( int receipt );

  // Fields
  static const char* EOL;
  std::shared_ptr<session> currentSession;
  std::thread     *iocRunnerThread;
  net::io_context *ioc;
  void (*messageHandler)( string str );
};


	       
