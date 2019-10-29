#include "StompClient.h"

void onMessage( std::string message )
{
  std::cout << "StompClient: received message " << message << std::endl;
}

int main( int argc, char *argv[] )
{
  // Create our Stomp Client
  StompClient stompy;

  // Connect to the server
  stompy.connect( "172.16.2.31", "8080", "/stomp-server", NULL, NULL );

  // Set up our message handler
  stompy.setMessageHandler( onMessage );

  // Subscribe to the topic
  stompy.subscribe( 147, "/topic/greetings", "auto" );

  // Send out our info
  stompy.send( "/app/hello", "application/JSON", "{ \"name\": \"Fred\" }" );

  // Wait for a message to appear
  stompy.synchronizeMessage();

  // We're done. Unsubscribe from the topic and ...
  stompy.unsubscribe( 147 );

  // ... disconnect from the WebSocket
  stompy.disconnect( 123 );

  // Wait for a receipt for the disconnection.
  stompy.synchronizeReceipt();
  
  // Close the WebSocket
  stompy.close();

  // Wait until the WebSocket's thread exits
  stompy.synchronize();		   
}
