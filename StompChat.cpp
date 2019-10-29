#include <cstdlib> 
#include <string> 
#include <gtk/gtk.h>
#include <thread>
#include <sstream>
#include <regex>
#include "StompClient.h"
using std::string;

// These are global so that they are easily accessible from the "sendText" callback.
GtkTextIter    iter;
GtkTextBuffer *buffer;
GtkWidget     *nameField;
StompClient    stompy;



void onMessage( string message )
{
  // Parse the JSON apart to get the message
  // The message is of the form
  /*
  { "message" : "message body" }
  */

  const char* messageText;
  std::regex  messageRegex("^.*:[[:space:]]*\"(.*)\".*$");
  std::smatch messageMatcher;
  std::string messageBody;
  if( std::regex_search( message, messageMatcher, messageRegex ))
  {
    messageBody = messageMatcher[1];
    std::cout << "From regex, message body is " << messageBody << std::endl;
    messageText = messageBody.c_str();
  }
  else
  {
    messageText = message.c_str();
  }

  g_print( "Got text from server: %s\n", messageText );

  gtk_text_buffer_insert( buffer, &iter, messageText, -1 );
  gtk_text_buffer_insert( buffer, &iter, "\n",        -1 );
}

// Send text to the server
void sendText( GtkEntry *entry, gpointer data )
{
  const char* newText = gtk_entry_get_text( GTK_ENTRY( entry ) );
  g_print( "Text in entry box: %s\n", newText );

  const char* sender = gtk_entry_get_text( GTK_ENTRY( nameField ) );
  g_print( "Sender name is: %s\n", sender );

  // Formulate the message in JSON Format
  std::ostringstream sendStream;
  sendStream << "{ ";
  if( sender != NULL && strlen( sender ) > 0 )
  {
    sendStream << "\"sender\" : \"" << sender << "\", ";
  }
  sendStream << "\"message\": \"" << newText << "\" }";

  std::string messageString = sendStream.str();
  std::cout << "Message from sstream is " << messageString << std::endl;
  const char *messageBody = messageString.c_str();

  g_print( "Message is %s\n", messageBody );
  
  // Send the message
  stompy.send( "/app/topic1", "application/JSON", messageBody );
  gtk_entry_set_text( entry, "" );
}

void destroy( GtkWidget *widget, gpointer data )
{
  std::cout << "Closing application" << std::endl;
  
  // Close the WebSocket connection
  stompy.unsubscribe( 147 );
  stompy.disconnect( 123 );
  stompy.synchronizeReceipt();

  // Now close the GTK application
  gtk_main_quit();
}

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

  // Create a name field
  nameField = gtk_entry_new();
  gtk_box_pack_start( GTK_BOX( vbox ), nameField, FALSE, FALSE, 0 );
  
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

  // Open the STOMP connection
  stompy.connect( "172.16.2.31", "8080", "/stomp-server", NULL, NULL );
  stompy.setMessageHandler( onMessage );

  // Subscribe our topic
  stompy.subscribe( 147, "/topic/topic1", "auto" );

  // Show everything
  gtk_widget_show_all( window );

  // Start the GTK part of the application.
  gtk_main();

  return 0;
}

  
  
  
