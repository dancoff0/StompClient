#include "StompClient.h"

int main( int argc, char *argv[] )
{
  // Create our Stomp Client
  StompClient stompy;

  // Connect to the server
  stompy.connect( "172.16.2.32", "8080", "/gs-guide-websocket", NULL, NULL );

  // Subscribe to the topic
  stompy.subscribe( 452, "/topic/greetings", "auto" );

  // Send out our info
  stompy.send( "/app/hello", "application/JSON", "{ \"name\": \"Fred\" }" );

  // Wait for a message to appear
  stompy.synchronizeMessage();

  // We're done. Unsubscribe from the topic and ...
  stompy.unsubscribe( 452 );

  // ... disconnect from the WebSocket
  stompy.disconnect( 123 );

  // Wait for a receipt for the disconnection.
  stompy.synchronizeReceipt();

  // Close the WebSocket
  stompy.close();

  // Wait until the WebSocket's thread exits
  stompy.synchronize();		   
}
