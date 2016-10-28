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
        if (page == 2 and !this->controlPanelDetected) {
          detectControlPanel();
        }
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

void PaulCommand::detectControlPanel() {
  this->controlPanelDetected = true;
  if (this->pages[2][14]==1) { //1=LED,0=TFT
    //LCD panel
    FANSPEED_PAGE = 2; //LED
    FANSPEED_POSITION = 12; //LED 
  } else {
    //TFT
    FANSPEED_PAGE = 0; //TFT
    FANSPEED_POSITION = 8; //TFT
  }
}

void PaulCommand::injectReply() {


  //master asks whether there is something new from control panel
  if (this->commandBuffer[2] == 1) {
    rs485transmitOn();
    if (FANSPEED_PAGE==0) { 
      reply0x00HasChanged(); // tft - yes, something has changed
    } else {
      reply0x20HasChanged(); // led - yes, something has changed
    }
    Serial1.flush();
    rs485receive();
    return;
  }

  //master asks for changed values on PAGE)
  if (this->commandBuffer[2] == 3 && this->commandBuffer[3]==FANSPEED_PAGE*16) { //data query on PAGE?
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
  this->pages[FANSPEED_PAGE][0] = 0x101; //to master
  this->pages[FANSPEED_PAGE][4+FANSPEED_POSITION] = this->fanSpeed; //1st data byte is on address 0x04
  this->pages[FANSPEED_PAGE][20] = xorFE(this->pages[FANSPEED_PAGE],20);
  for(int i=0; i<21; i++) {
    Serial1.write(this->pages[FANSPEED_PAGE][i]);
  }
  Serial.write("-replyPage0x"); Serial.print(FANSPEED_PAGE*16, HEX); Serial.write("-");        
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

//xor - computes 0xFE XOR on given array[0 to length-1] 
byte PaulCommand::xorFE(word arrayXor[], int arrayLength) {
   byte checksum = 0xFE;
   for(int i=0; i<arrayLength; i++) {
    checksum = checksum ^ (arrayXor[i] & 0xFF); //checkum on lower 8 bit!
   }
  return checksum;
}

void PaulCommand::changeFanSpeed(int fanSpeed) {
  Serial.print(F("changeFanSpeed"));
  this->requestChangeFanSpeed = true; //start 
  relayOn();
  this->fanSpeed = fanSpeed;
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
