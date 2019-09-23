#include <Lanc.h>
//#include <SoftwareSerial.h>
Lanc CLanc;
//SoftwareSerial mySerial(1,0);


void setup() {
  // initialize serial communication at 9600 bits per second:
  Serial.begin(9600);
//  mySerial.begin(9600);
  CLanc.init(10);
}

void loop() {
  // put your main code here, to run repeatedly:
  // read the input on analog pin 0:
  int sensorValue = analogRead(A0);
//  char c = mySerial.read();
  char l = char(CLanc.LANC_Frame[0]);
  // print out the value you read:
  Serial.println("Frame 0: "+l);
//  delay(1);        // delay in between reads for stability
}
