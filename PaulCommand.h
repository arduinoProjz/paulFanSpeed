#ifndef PaulCommand_h
#define PaulCommand_h
#include <Arduino.h>

/*
 * PaulCommand.h
 */
class PaulCommand
{
  static const int COMMAND_MAX_SIZE = 21; //max.length of commmand

  // fan speed change configuration for LCD panel and TFT touch screen
  // LCD panel replies fan speed on 3rd page (0x20) and 13th (Data12) position
  // TFT touchscreen replies fan speed on 1st page (0x00) and 9th (Data8) position  
  static const int PAGE = 0x20; //LED
  static const int FANSPEED_POSITION = 12; //LED

  //static const int PAGE = 0x00; //TFT
  //static const int FANSPEED_POSITION = 8; //TFT

  private:

    word commandBuffer[COMMAND_MAX_SIZE]; //longest command is 21 byte long
    // commandBuffer[0] - address - eg. 0x102,0x101
    // commandBuffer[1] - command length - eg. 00,01, 0A
    // commandBuffer[2] - command type
    int commandIndex = 0; //index in command, next byte will be written on this index
    bool commandStarted = false;

    word adress0x20buff[COMMAND_MAX_SIZE]; //place to data from adress 0x20, will be filled when master send 
    //it to cp and reused on change fan speed reply

    static const int MAXPAGES = 6;
    word pages[MAXPAGES][COMMAND_MAX_SIZE]; // max 16 pages 
    
    void logCommand();
    void replyAck();
    void replyNoChange();
    void reply0x00HasChanged();
    void reply0x20HasChanged();
    void replyPage();
    void injectReply(); //injects reply to bus

    int replyLength = 0; //length of reply command (position 0 to replyLength-1)

    byte xorFE(int bytesCount);

    bool requestChangeFanSpeed = false; // true=request to change
    int fanSpeed = 3; //1-7

    void relayOn();
    void relayOff();
    void rs485transmitOn();
    void rs485receive();

  public:
    PaulCommand();

    void addWordToMessage(word messageWord);

    word getAdress0x20buff(int index); //return one word from 0x20 Buffer, index = 0 - COMMAND_MAX_SIZE 
    word getPageData(int page, int data);
    void changeFanSpeed(int fanSpeed); //1-7
    void init();
};

#endif

