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
 scrollText(" { } [ ] ~ ! @ # $ % ^ & * ( ) _ - = + / ' ? ; : > < | a b c d e f g h i j k l m n o p q r s t u v w x y z ");
//displayText("hello");
}

