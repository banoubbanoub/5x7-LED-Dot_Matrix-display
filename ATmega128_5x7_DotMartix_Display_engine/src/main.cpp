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
 scrollText("h #");
//displayText("hello");
}

