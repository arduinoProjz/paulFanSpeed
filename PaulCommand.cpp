#include <Arduino.h>
#include "PaulCommand.h"

/* 
 * PaulCommand.cpp
 * 
 * functions
 * listens on bus
 * reads control panel messages
 * saves register values
 * injects responses to bus
 */
 
//

#define SerialDeControl 46   //RS485 Direction control
#define RelayControl1 44   // Relay Open Closed control
#define RelayControl2 45   // Relay Open Closed control
#define RS485Transmit    HIGH
#define RS485Receive     LOW

//
// Constructor
//
PaulCommand::PaulCommand()
{
}

void PaulCommand::init() {
  //init rs485, relay
  pinMode(SerialDeControl, OUTPUT);
  pinMode(RelayControl1, OUTPUT);
  pinMode(RelayControl2, OUTPUT);
  rs485receive();
  relayOff();
}

// process one word/byte from serial
void PaulCommand::addWordToMessage(word messageWord) {
  if (messageWord == 0x102) // 0x102 = start of new message for control panel
  {
    //Serial.write("start message for 0x102;");   
    this->commandIndex = 0; //reset input buffer
    this->commandStarted = true; //start of comm found
  }
  if (this->commandStarted && this->commandIndex < COMMAND_MAX_SIZE) //do not overflow
  {
    this->commandBuffer[this->commandIndex++] = messageWord; //save word, increment index
    //is message for 0x102 completed?
    if ((this->commandIndex>1) && (this->commandIndex == this->commandBuffer[1] + 4)) { // 4 bytes extra: adress, lenght, command, checksum
      //logCommand();
      //is it config info to save for reply?
      if (this->commandBuffer[2] == 4 && this->commandBuffer[3] >=0 && this->commandBuffer[3] <MAXPAGES*16 && this->commandBuffer[1] == 0x11) { // saves pages 0x00 - 0x50, length should be 0x11
        //todo checksum!
        int page = this->commandBuffer[3] / 16;
        memcpy(this->pages[page], this->commandBuffer, 21*2); //save page 0xxx (params: dest, source, length)
        Serial.print(F("-savePage"));
        Serial.print(this->commandBuffer[3], HEX);
        Serial.println(F("-"));
      }
      //request for fan speed change in progess? 
      if (this->requestChangeFanSpeed) {  
        //injects reply to bus
        injectReply();
      }
    } 
  }
}

void PaulCommand::injectReply() {


  //master asks whether there is something new from control panel
  if (this->commandBuffer[2] == 1) {
    rs485transmitOn();
    if (PAGE==0x00) { 
      reply0x00HasChanged(); // tft - yes, something has changed
    } else {
      reply0x20HasChanged(); // led - yes, something has changed
    }
    Serial1.flush();
    rs485receive();
    return;
  }

  //master asks for changed values on PAGE)
  if (this->commandBuffer[2] == 3 && this->commandBuffer[3]==PAGE) { //data query on PAGE?
    rs485transmitOn();
    replyPage(); // reply changed value
    Serial1.flush();
    rs485receive();
    this->requestChangeFanSpeed = false; 
    relayOff(); //we are done, set shield back to passive mode
    return;
  }
}

void PaulCommand::reply0x00HasChanged() {
  Serial1.write(0x101);
  Serial1.write(0x02);
  Serial1.write(0x02);
  Serial1.write(0x82); //change on adress 0x00 (tft)
  Serial1.write(0x10);
  Serial1.write(0x6D);
  Serial.write("-reply0x00HasChanged-");        
}

void PaulCommand::reply0x20HasChanged() {
  Serial1.write(0x101);
  Serial1.write(0x02);
  Serial1.write(0x02);
  Serial1.write(0x92); //change on adress 0x20 (lcd)
  Serial1.write(0x10);
  Serial1.write(0x7D);
  Serial.write("-reply0x20HasChanged-");        
}

void PaulCommand::replyPage() {
  //manipulate destadress and fanspeed, xor
  this->adress0x20buff[0] = 0x101; //to master
  this->adress0x20buff[4+FANSPEED_POSITION] = this->fanSpeed; //1st data byte is on address 0x04
  this->adress0x20buff[20] = xorFE(20);
  for(int i=0; i<21; i++) {
    Serial1.write(this->adress0x20buff[i]);
  }
  Serial.write("-replyPage"); Serial.print(PAGE, HEX); Serial.write("-");        
}

//helper methods
void PaulCommand::logCommand() {
  Serial.print("commandlog:");
  for (int i = 0; i < this->commandIndex; i++) {
    Serial.print(this->commandBuffer[i],  HEX);
    Serial.print(" ");
  }
  Serial.println();
}

//xor - computes 0xFE XOR on replyBuffer[0 to bytesCount-1] 
byte PaulCommand::xorFE(int bytesCount) {
   byte checksum = 0xFE;
   for(int i=0; i<bytesCount; i++) {
    checksum = checksum ^ (this->adress0x20buff[i] & 0xFF); //checkum on lower 8 bit!
   }
  return checksum;
}

void PaulCommand::changeFanSpeed(int fanSpeed) {
  Serial.print(F("changeFanSpeed"));
  this->requestChangeFanSpeed = true; //start 
  relayOn();
  this->fanSpeed = fanSpeed;
}

word PaulCommand::getAdress0x20buff(int index) {
  if (index>=0 && index< COMMAND_MAX_SIZE) {
    return this->adress0x20buff[index]; 
  }
  return 0;
}

word PaulCommand::getPageData(int page, int data) {
  if (page>=0 && page<MAXPAGES && data>=0 && data< COMMAND_MAX_SIZE) {
    return this->pages[page][data];
  }
  return 0;
}

// arduino will acts as a led/tft panel
void PaulCommand::relayOn() {
  //Serial.print("relayOn");
  digitalWrite(RelayControl1, false);  // false = open, led/tft panel is disconnected from rs485 bus
  digitalWrite(RelayControl2, false); 
}

// arduino goes in passive mode
void PaulCommand::relayOff() {
  //Serial.print("relayOff");
  digitalWrite(RelayControl1, true);  // true = closed, led/tft is again on bus
  digitalWrite(RelayControl2, true); 
}

//set rs485 to transmit on, no other should transmit in thss time!
void PaulCommand::rs485transmitOn() {
  digitalWrite(SerialDeControl, RS485Transmit); 
}

// end transmit mode. master can then transmit
void PaulCommand::rs485receive() {
  digitalWrite(SerialDeControl, RS485Receive);  
}
