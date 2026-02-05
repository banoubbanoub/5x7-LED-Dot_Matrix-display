#include <Arduino.h>

#include <Display_engine.h>

void setup() {
  // put your setup code here, to run once:
  intPort();
  columnsOff();
}

void loop()
{
 

refreshDisplay();
 scrollText(" WELCOME HAVE FUN HELLO :) GOOD LUCK THANK YOU  ");
//displayText("hello world");
}

