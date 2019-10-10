#pragma once

class websocketcallbacks
{
 public:
  virtual void onRead( char const* message ) = 0;
};
