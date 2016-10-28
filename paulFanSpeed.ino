#include "PaulCommand.h"
#include <SPI.h>
#include <Ethernet.h>

/* 
 * sketch for paul FOCUS200 ventilation unit to control fan speed over HTTP
 * Oct 2016, v0.1
 * 
 * prerequestments HW
 * mega board, 
 * ethernet shield,
 * custom "paul" shield (max485 ttl to rs485 converter, 2channel relay module)   
 * 
 * prerequestments SW
 * Arduino HarwareSerial 9N1 lib (copy files from hwSerial9n1.zip into /hardware/arduino/avr/cores/arduino)
 * 
*/

/*  
 * mega:  
 * The Arduino Mega has three additional serial ports: 
 * Serial1 on pins 19 (RX) and 18 (TX), 
 * Serial2 on pins 17 (RX) and 16 (TX), 
 * Serial3 on pins 15 (RX) and 14 (TX). 
 * 
 * RS485 converter:  
 *  19 (RX) to pin RO
 *  18 (TX) to pin DI
 *  
 * 2Relay Module:
 * active Low!
 * 
 * Ethernet shield
 * Arduino communicates with the shield using the SPI bus. This is on digital pins 11, 12, and 13 
 * on the Uno and pins 50, 51, and 52 on the Mega. On both boards, pin 10 is used as SS. On the Mega, 
 * the hardware SS pin, 53, is not used to select the W5100, but it must be kept as an output or 
 * the SPI interface won't work.
 * 
 * Ethernet shield attached to pins 10, 11, 12, 13
 * 
 * http get und json examples: 
 * http://arduinobasics.blogspot.co.at/2015/11/get-arduino-data-over-internet-using.html
 * http://arduino.stackexchange.com/questions/1092/parse-json-with-arduino-to-turn-on-led
 * 
*/

/*-----( Declare objects )-----*/
PaulCommand paulCommand;

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
//IPAddress ip(169, 254, 237, 177);
IPAddress ip(10, 0, 0, 177);

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(80);

void setup()   /****** SETUP: RUNS ONCE ******/
{
  // Start the built-in serial port, probably to Serial Monitor
  Serial.begin(9600);
  //while (! Serial); // Wait until Serial is ready
  Serial.println(F("*** paul novus/focus relay hack ;-)"));

  //init paul ventilation shield
  paulCommand.init();
  Serial1.begin(9600, SERIAL_9N1);   // set the data rate 9600 and 9bit, no parity, one stop bit, see HardwareSerial

  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());

}//--(end setup )---


void loop()   /****** LOOP: RUNS CONSTANTLY ******/
{
  checkClient();
  delay(1); //1msec delay makes miracle!!
}//--(end main loop )---

// event handler on serial1
void serialEvent1(){
  word readWord = Serial1.read();
  //Serial.print(readWord, HEX);
  //Serial.print(",");
  paulCommand.addWordToMessage(readWord); 
}

/* 
 *  change fan speed over serial, type 1 to 7 to change fan speed
 *  add checkSerial(); to loop method
 *  
void checkSerial() {
  if (Serial.available()) {
    int fanSpeed = Serial.read()-48;
    if (fanSpeed >=1 and fanSpeed <= 7) {
      Serial.print("changeFanSpeed ");
      Serial.println(fanSpeed);
      paulCommand.changeFanSpeed(fanSpeed);
    }
  }
}
*/

void checkClient() {
  EthernetClient client = server.available();      // assign any newly connected Web Browsers to the "client" variable.
  
  if(client){
    Serial.println(F("Client Connected"));
    
    boolean done = false;
    while (client.connected() && !done) 
      {
      while (client.available () > 0 && !done)
        done = processIncomingByte (client.read ());
      }  // end of while client connected
    
    
    //Send the Server response header back to the browser.
    client.println(F("HTTP/1.1 200 OK"));           // This tells the browser that the request to provide data was accepted
    client.println(F("Access-Control-Allow-Origin: *"));  //Tells the browser it has accepted its request for data from a different domain (origin).
    client.println(F("Content-Type: application/json; charset=utf-8"));  //Lets the browser know that the data will be in a JSON format
    client.println(F("Server: Arduino"));           // The data is coming from an Arduino Web Server (this line can be omitted)
    client.println(F("Connection: close"));         // Will close the connection at the end of data transmission.
    client.println();                            // You need to include this blank line - it tells the browser that it has reached the end of the Server reponse header.
    
    //Transmit the register 0x20 values (data 0 - 15) to the Web Browser in JSON format
    //Example Transmission: [{0, 0}, {1, 54}, {2,1} ...]
    client.print(F("["));                           // This is tha starting bracket of the JSON data
    for(int i=0; i<16; i++){                     
      client.print(F("{"));
      client.print(i);                           
      client.print(F(","));
      client.print(paulCommand.getAdress0x20buff(i+4), HEX);               // data byte starts at offset 4 in message
      if(i==15){
        client.print(F("}"));                       // The last value will end with a bracket (without a comma)
      } else {
        client.print(F("},"));                      // All other values will have a comma after the bracket.
      }
    }
    client.println(F("]"));                         // This is the final bracket of the JSON data
    delay(10);                                     // give the web browser time to receive the data
    client.stop();                               // This method terminates the connection to the client
    Serial.println(F("Client has closed"));         // Print the message to the Serial monitor to indicate that the client connection has closed.
  }
}

// how much serial data we expect before a newline
const unsigned int MAX_INPUT = 100;
// the maximum length of paramters we accept
const int MAX_PARAM = 10;

// Example GET line: GET /?f1 HTTP/1.1 
//http://10.0.0.177/?f5
// fanSpeed = 1
void processGet (const char * data)
  {
  // find where the parameters start
  const char * paramsPos = strchr (data, '?');
  if (paramsPos == NULL)
    return;  // no parameters
  // find the trailing space
  const char * spacePos = strchr (paramsPos, ' ');
  if (spacePos == NULL)
    return;  // no space found
  // work out how long the parameters are
  int paramLength = spacePos - paramsPos - 1;
  // see if too long
  if (paramLength >= MAX_PARAM)
    return;  // too long for us
  // copy parameters into a buffer
  char param [MAX_PARAM];
  memcpy (param, paramsPos + 1, paramLength);  // skip the "?"
  param [paramLength] = 0;  // null terminator
  
  // do things depending on argument (GET parameters)

  if (paramLength == 2) {
    if (param [0] == 'f' && param [1] >= '1' && param [1] <= '7') {
      //fan speed 1 to 7
      int fanSpeed = param [1]-'0';
      Serial.print (F("--set fan speed "));
      Serial.println  (fanSpeed); 
      paulCommand.changeFanSpeed(fanSpeed);
    }
  }   

  }  // end of processGet

// here to process incoming serial data after a terminator received
void processData (const char * data)
  {
  Serial.println (data);
  if (strlen (data) < 4)
    return;

  if (memcmp (data, "GET ", 4) == 0)
    processGet (&data [4]);
  }  // end of processData

bool processIncomingByte (const byte inByte)
  {
  static char input_line [MAX_INPUT];
  static unsigned int input_pos = 0;
  switch (inByte)
    {
    case '\n':   // end of text
      input_line [input_pos] = 0;  // terminating null byte
      if (input_pos == 0){
        return true;   // got blank line
      }
      // terminator reached! process input_line here ...
      processData (input_line);
      // reset buffer for next time
      input_pos = 0;  
      break;

    case '\r':   // discard carriage return
      break;

    default:
      // keep adding if not full ... allow for terminating null byte
      if (input_pos < (MAX_INPUT - 1))
        input_line [input_pos++] = inByte;
      break;
    }  // end of switch
  return false;    // don't have a blank line yet
  } // end of processIncomingByte
    
